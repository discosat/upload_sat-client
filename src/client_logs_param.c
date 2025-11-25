#include <param/param.h>

#include "client_logs_param.h"
#include "vmem_storage.h"

// Define params here...

PARAM_DEFINE_STATIC_VMEM(CLIENT_STATUS_LOG, get_upload_log_status, PARAM_TYPE_UINT32, -1, 0, PM_READONLY, NULL, NULL, storage, VMEM_UPLOAD_LOG_ADDR, "Latest upload log code");
