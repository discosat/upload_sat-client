#include <param/param.h>
#include <param/param_list.h>

#include "client_logs_param.h"

static uint32_t _get_upload_log_status = 0;

PARAM_DEFINE_STATIC_RAM(CLIENT_STATUS_LOG, get_upload_log_status, PARAM_TYPE_UINT32, -1, 0, PM_READONLY, NULL, NULL, &_get_upload_log_status, "Latest upload log code");

// Registration function
void client_logs_param_init(void)
{
    param_list_add(&get_upload_log_status);
}

// Helper to set value
void set_client_log_status(uint32_t status) {
    _get_upload_log_status = status;
}