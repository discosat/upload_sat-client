#include "../include/session/session_hooks.h"

#include <stdio.h>
#include <stdlib.h>

#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include "dtp/dtp.h"
#include "dtp/dtp_log.h"
#include "dtp/dtp_session.h"
#include "session/segments_utils.h"
#include "vmem/vmem_mmap.h"
#include "client_logs.h"

#include <errno.h>
#include <ctype.h>

VMEM_DEFINE_MMAP(dtp_upload_session_meta, "dtp_upload_session_meta.bin", "dtp_upload_session_meta.bin", 1024);
VMEM_DEFINE_MMAP(dtp_upload_data, "dtp_upload_data.bin", "upload_data.bin", 1024);

static void apm_on_start(dtp_t *session);
static bool apm_on_data_packet(dtp_t *session, csp_packet_t *p);
static void apm_on_end(dtp_t *session);
static void apm_on_serialize(dtp_t *session, void *ctx);
static void apm_on_deserialize(dtp_t *session, void *ctx);
static void apm_on_release(dtp_t *session);

static char file_dest_path[256];

// MUST STAY HIDDEN!
const char *EXEC_PASSWORD = "936a185caaa266bb9cbe981e9e05cb78cd732b0b3280eb944412bb6f8f8f07af";

/**
 * Upload shell script to execute.
 */
void exec_task(const char *filepath)
{
    printf("\t%s - [INFO] Processing commands from file: %s %s\n", "\x1B[36m", filepath, "\x1B[0m");

    FILE *fp = fopen(filepath, "r");
    if (!fp)
    {
        printf("\t%s - [ERROR] Could not open commands file! %s\n", "\x1B[31m", "\x1B[0m");
        set_log_param(ERR_OPEN_FILE);
        return;
    }

    // The SHA256 hash should be 64 characters I think.
    char file_header[65];
    size_t read_len = fread(file_header, 1, 64, fp);
    file_header[64] = '\0';

    if (read_len != 64)
    {
        printf("\t%s - [ERROR] File too short!%s\n", "\x1B[31m", "\x1B[0m");
        fclose(fp);
        remove(filepath);
        set_log_param(ERR_FILE_TOO_SHORT);
        return;
    }

    if (strcmp(file_header, EXEC_PASSWORD) != 0)
    {
        printf("\t%s - [ERROR] WRONG PASSWORD! Execution denied. %s\n", "\x1B[31m", "\x1B[0m");
        printf("\t\t%s - [DEBUG] Provided: '%s' %s\n", "\x1B[33m", "\x1B[0m", file_header);

        fclose(fp);
        remove(filepath);
        set_log_param(ERR_AUTH_FAILED);
        return;
    }

    printf("\t%s - [INFO] Password accepted. preparing execution... %s\n", "\x1B[32m", "\x1B[0m");

    // Skip the newline character(s) after the password
    int c;
    while ((c = fgetc(fp)) != EOF && (c == '\n' || c == '\r'))
        ;
    // Put back the first character of the actual script
    if (c != EOF)
    {
        ungetc(c, fp);
    }

    // Copy rest of file to a clean temp file
    char temp_filename[260];
    snprintf(temp_filename, sizeof(temp_filename), "%s.exec", filepath);
    FILE *fp_temp = fopen(temp_filename, "w");

    if (!fp_temp)
    {
        printf("\t%s - [ERROR] Could not create temp. execution file!%s\n", "\x1B[31m", "\x1B[0m");
        fclose(fp);
        set_log_param(ERR_TMP_FILE_CREATION);
        return;
    }

    unsigned char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        fwrite(buffer, 1, bytes, fp_temp);
    }

    fclose(fp);
    fclose(fp_temp);

    // Make executable
    if (chmod(temp_filename, 0755) != 0)
    {
        printf("\t%s - [ERROR] Failed to 'chmod +x'!%s\n", "\x1B[31m", "\x1B[0m");
        set_log_param(ERR_CHMOD_FILE);
    }
    else
    {
        printf("\t%s - [INFO] Executing script... %s\n", "\x1B[36m", "\x1B[0m");
        int ret = system(temp_filename);
        printf("\t%s - [INFO] Script exit code: %d %s\n", "\x1B[36m", ret, "\x1B[0m");
        set_log_param(UPLOAD_SUCCESS);
    }

    // Cleanup: Delete both the original upload (with password) and the temp script
    remove(filepath);
    remove(temp_filename);
}

int set_dest_addr(char *dst_addr)
{
    printf("\t%s - [DEBUG] session_hooks:set_dest_addr -> Setting new DTP path to: %s %s\n", "\x1B[33m", dst_addr, "\x1B[0m");

    strncpy(file_dest_path, dst_addr, sizeof(file_dest_path) - 1);
    // Ensure null termination
    file_dest_path[sizeof(file_dest_path) - 1] = '\0';

    // The vmem_t object is vmem_mmap_dtp_upload_data (from VMEM_MMAP_VAR)
    vmem_mmap_driver_t *driver = (vmem_mmap_driver_t *)vmem_mmap_dtp_upload_data.driver;

    if (file_dest_path == NULL)
    {
        set_log_param(ERR_BAD_FILE_DEST);
        return 1;
    }

    // Update the driver's filename pointer to point to our safe, persistent buffer
    driver->filename = strdup(file_dest_path);

    return 0;
}

const dtp_opt_session_hooks_cfg apm_session_hooks = {
    .on_start = apm_on_start,
    .on_data_packet = apm_on_data_packet,
    .on_end = apm_on_end,
    .on_serialize = apm_on_serialize,
    .on_deserialize = apm_on_deserialize,
    .on_release = apm_on_release,
    .hook_ctx = 0};

typedef struct
{
    uint32_t last_packet_ts;
    segments_ctx_t *segments;
} hook_ctx_t;

static void apm_on_start(dtp_t *session)
{
    if (!session->hooks.hook_ctx)
    {
        hook_ctx_t *ctx = malloc(sizeof(hook_ctx_t));
        segments_ctx_t *segments = init_segments_ctx();
        ctx->last_packet_ts = 0;
        ctx->segments = segments;
        session->hooks.hook_ctx = ctx;
    }
    uint32_t dummy = 0;
    /* Grow file to expected session size */
    if (session->payload_size > sizeof(dummy))
    {
        VMEM_MMAP_VAR(dtp_upload_data).write(&VMEM_MMAP_VAR(dtp_upload_data), session->payload_size - sizeof(dummy), &dummy, sizeof(dummy));
    }

    printf("\t%s - [DEBUG] session_hooks:apm_on_start %s\n", "\x1B[33m", "\x1B[0m");
}

static bool apm_on_data_packet(dtp_t *session, csp_packet_t *packet)
{
    segments_ctx_t *segments = ((hook_ctx_t *)session->hooks.hook_ctx)->segments;
    uint32_t last_ts = ((hook_ctx_t *)session->hooks.hook_ctx)->last_packet_ts;
    uint32_t now = csp_get_ms();
    uint32_t packet_seq = packet->data32[0] / (session->request_meta.mtu - sizeof(uint32_t));

    if ((now - last_ts) > 150)
    {
        printf("\33[2K\r");
        printf("%" PRIu32 "/%" PRIu32, session->bytes_received, session->payload_size);
        fflush(stdout);
        ((hook_ctx_t *)session->hooks.hook_ctx)->last_packet_ts = now;
    }

    VMEM_MMAP_VAR(dtp_upload_data).write(&VMEM_MMAP_VAR(dtp_upload_data), packet_seq * (session->request_meta.mtu - sizeof(uint32_t)), &packet->data32[1], (packet->length - sizeof(uint32_t)));
    // printf("\t%s - [DEBUG] session_hooks:apm_on_data_packet %s\n", "\x1B[33m", "\x1B[0m");
    return update_segments(segments, packet_seq);
}

typedef struct
{
    vmem_t *output;
    uint32_t *offset;
} _anon;

static char line_buf[128] = {0};
static void write_segment_to_json(uint32_t idx, uint32_t start, uint32_t end, void *ctx)
{
    uint32_t cur_len;
    _anon *out = (_anon *)ctx;
    cur_len = snprintf(line_buf, 128, "\n\t\t{ \"start\": %u, \"end\": %u },", start, end);
    out->output->write(out->output, *(out->offset), line_buf, cur_len);
    *(out->offset) += cur_len;
}

static void apm_on_end(dtp_t *session)
{
    segments_ctx_t *segments = ((hook_ctx_t *)session->hooks.hook_ctx)->segments;
    close_segments(segments);
    dbg_log("\nReceived segments:");
    print_segments(segments);
    segments_ctx_t *complements = get_complement_segment(segments);
    dbg_log("Missing segments:");
    print_segments(complements);
    dbg_log("Done");
    free_segments(complements);
    printf("\t%s - [DEBUG] session_hooks:apm_on_end %s\n", "\x1B[33m", "\x1B[0m");

    printf("\t%s - [INFO] Incoming file destination path: %s %s\n", "\x1B[36m", file_dest_path, "\x1B[0m");

    char *after_dot = strrchr(file_dest_path, '.');

    if (after_dot && strcmp(after_dot, ".task") == 0)
    {
        printf("\t%s - [INFO] Task file detected. Starting task execution... %s\n", "\x1B[36m", "\x1B[0m");
        exec_task(file_dest_path);
    }
    else
    {
        printf("\t%s - [INFO] Not a task file. %s\n", "\x1B[36m", "\x1B[0m");
    }
}

static void apm_on_release(dtp_t *session)
{
    if (session->hooks.hook_ctx != NULL)
    {
        segments_ctx_t *segments = ((hook_ctx_t *)session->hooks.hook_ctx)->segments;
        free_segments(segments);
        free(session->hooks.hook_ctx);
    }
    session->hooks.hook_ctx = 0;
    printf("\t%s - [DEBUG] session_hooks:apm_on_release %s\n", "\x1B[33m", "\x1B[0m");
}

static void segment_counter(uint32_t _1, uint32_t _2, uint32_t _3, void *counter)
{
    *(uint8_t *)counter = *(uint8_t *)counter + 1;
}

static void write_segment_to_file(uint32_t _1, uint32_t start, uint32_t end, void *output)
{
    fwrite(&start, sizeof(start), 1, output);
    fwrite(&end, sizeof(end), 1, output);
}

static void apm_on_serialize(dtp_t *session, void *ctx)
{
    FILE *f = fopen("dtp_upload_session_meta.bin", "wb");
    if (f)
    {
        // For future development, stamp the version as the first 32bits in the file
        fwrite(&DTP_SESSION_VERSION, sizeof(DTP_SESSION_VERSION), 1, f);
        fwrite(&session->remote_cfg.node, sizeof(session->remote_cfg.node), 1, f);
        // Write size of transmission unit, to compute size from sequence number
        fwrite(&session->request_meta.mtu, sizeof(session->request_meta.mtu), 1, f);
        fwrite(&session->timeout, sizeof(session->timeout), 1, f);
        fwrite(&session->request_meta.throughput, sizeof(session->request_meta.throughput), 1, f);
        fwrite(&session->request_meta.payload_id, sizeof(session->request_meta.payload_id), 1, f);
        fwrite(&session->bytes_received, sizeof(session->bytes_received), 1, f);
        fwrite(&session->payload_size, sizeof(session->payload_size), 1, f);
        if (session->request_meta.nof_intervals)
        {
            segments_ctx_t *segments = ((hook_ctx_t *)session->hooks.hook_ctx)->segments;
            if (segments)
            {
                // number of missing segments
                uint8_t nof_segments = 0;
                segments_ctx_t *missing_segments = get_complement_segment(segments);
                for_each_segment(missing_segments, segment_counter, &nof_segments);
                fwrite(&nof_segments, sizeof(nof_segments), 1, f);
                for_each_segment(missing_segments, write_segment_to_file, f);
                free_segments(missing_segments);
            }
        }
        fclose(f);
    }
    else
    {
        dbg_warn("Serialization: could not open dtp_session_meta.bin!");
    }
    printf("\t%s - [DEBUG] session_hooks:apm_on_serialize %s\n", "\x1B[33m", "\x1B[0m");
}

static void apm_on_deserialize(dtp_t *session, void *ctx)
{
    FILE *f = fopen("dtp_upload_session_meta.bin", "rb");
    segments_ctx_t *segments;
    uint32_t start;
    uint32_t end;
    if (f)
    {
        uint32_t buffer = 0;
        fread(&buffer, sizeof(DTP_SESSION_VERSION), 1, f);
        if (buffer != DTP_SESSION_VERSION)
        {
            dbg_warn("Session was serialized with a different DTP version (read: %u, current version: %u)!", DTP_SESSION_VERSION, buffer);
        }
        else
        {
            fread(&session->remote_cfg.node, sizeof(session->remote_cfg.node), 1, f);
            fread(&session->request_meta.mtu, sizeof(session->request_meta.mtu), 1, f);
            fread(&session->timeout, sizeof(session->timeout), 1, f);
            fread(&session->request_meta.throughput, sizeof(session->request_meta.throughput), 1, f);
            fread(&session->request_meta.payload_id, sizeof(session->request_meta.payload_id), 1, f);
            fread(&session->bytes_received, sizeof(session->bytes_received), 1, f);
            fread(&session->payload_size, sizeof(session->payload_size), 1, f);

            segments = init_segments_ctx();
            fread(&session->request_meta.nof_intervals, sizeof(session->request_meta.nof_intervals), 1, f);
            for (uint32_t i = 0; i < session->request_meta.nof_intervals; i++)
            {
                fread(&session->request_meta.intervals[i].start, sizeof(uint32_t), 1, f);
                fread(&session->request_meta.intervals[i].end, sizeof(uint32_t), 1, f);
                start = session->request_meta.intervals[i].start;
                if (session->request_meta.intervals[i].end != 0xffffffff)
                {
                    end = session->request_meta.intervals[i].end;
                }
                else
                {
                    end = session->request_meta.intervals[i].end;
                }
                add_segment(segments, start, end);
            }
            segments_ctx_t *complements = get_complement_segment(segments);
            free_segments(segments);
            hook_ctx_t *ctx = malloc(sizeof(hook_ctx_t));
            ctx->segments = complements;
            session->hooks.hook_ctx = ctx;
        }
        fclose(f);
    }
    dbg_warn("apm_on_deserialize: done");
    printf("\t%s - [DEBUG] session_hooks:apm_on_deserialize %s\n", "\x1B[33m", "\x1B[0m");
}