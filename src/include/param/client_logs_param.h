#ifndef CLIENT_LOGS_PARAM_H
#define CLIENT_LOGS_PARAM_H

#include <param/param.h>
#include "client_logs_paramids.h"
#include "vmem_storage.h"

// Define params here...

PARAM_DEFINE_STATIC_VMEM(STATUS_LOG, upload_log_status, PARAM_TYPE_UINT32, -1, 0, PM_CONF, NULL, NULL, storage, VMEM_DOWNLOAD_LOG_ADDR, "Latest upload log code");

#endif