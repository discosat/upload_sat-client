#include <param/param.h>
#include <stdio.h>
#include "client_logs.h"
#include "client_logs_param.h"
#include "client_logs_paramids.h"
#include "vmem_storage.h"

void set_log_param(LOG_CODE log_code)
{
    uint32_t get_code_val = (uint32_t)log_code;
    param_set_uint32(&get_upload_log_status, get_code_val);
    if (get_code_val != UPLOAD_SUCCESS) {
        printf("\t%s - [ERROR] Logged error code: %d %s\n", "\x1B[31m", get_code_val, "\x1B[0m");
    }
}