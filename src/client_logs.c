#include <param/param.h>
#include <stdio.h>
#include "client_logs.h"
#include "client_logs_param.h"
#include "client_logs_paramids.h"
#include "vmem_storage.h"

PARAM_DEFINE_STATIC_VMEM(CLIENT_STATUS_LOG, get_upload_log_status, PARAM_TYPE_UINT32, PARAM_MAX_SIZE, 0, PM_READONLY, NULL, NULL, storage, VMEM_UPLOAD_LOG_ADDR, "Latest upload log code");

void set_log_param(CLIENT_LOG_CODE log_code)
{
    uint32_t get_code_val = (uint32_t)log_code;
    param_set_uint32(&get_upload_log_status, get_code_val);
    if (get_code_val != UPLOAD_SUCCESS) {
        printf("\t%s - [ERROR] Logged error code: %d %s\n", "\x1B[31m", get_code_val, "\x1B[0m");
    }
}

void client_logs_init(void)
{
    param_set_uint32(&get_upload_log_status, 0);
    param_t *get_upload_log_status_param = &get_upload_log_status;
    printf("\t%s - [INFO] client_logs:client_logs_init %s\n", "\x1B[36m", "\x1B[0m");
    printf("\t%s - [INFO] get_upload_log_status->vaddr: %lu %s\n", "\x1B[36m", get_upload_log_status_param->vaddr, "\x1B[0m");
}