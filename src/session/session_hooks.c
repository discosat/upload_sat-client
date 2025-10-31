#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include "dtp/dtp.h"
#include "dtp/dtp_log.h"
#include "dtp/dtp_session.h"
#include "session/segments_utils.h"
#include "vmem/vmem_mmap.h"

VMEM_DEFINE_MMAP(dtp_session_meta, "dtp_session_meta.bin", "dtp_session_meta.bin", 1024);
VMEM_DEFINE_MMAP(dtp_data, "dtp_data.bin", "/home/burak/Documents/DISCO-2/uploaded_stuff.bin", 1024);

static void apm_on_start(dtp_t *session);
static bool apm_on_data_packet(dtp_t *session, csp_packet_t *p);
static void apm_on_end(dtp_t *session);
static void apm_on_serialize(dtp_t *session, void *ctx);
static void apm_on_deserialize(dtp_t *session, void *ctx);
static void apm_on_release(dtp_t *session);

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
        VMEM_MMAP_VAR(dtp_data).write(&VMEM_MMAP_VAR(dtp_data), session->payload_size - sizeof(dummy), &dummy, sizeof(dummy));
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

    VMEM_MMAP_VAR(dtp_data).write(&VMEM_MMAP_VAR(dtp_data), packet_seq * (session->request_meta.mtu - sizeof(uint32_t)), &packet->data32[1], (packet->length - sizeof(uint32_t)));
    printf("\t%s - [DEBUG] session_hooks:apm_on_data_packet %s\n", "\x1B[33m", "\x1B[0m");
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
    FILE *f = fopen("dtp_session_meta.bin", "wb");
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
    FILE *f = fopen("dtp_session_meta.bin", "rb");
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