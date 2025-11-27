#ifndef CLIENT_LOGS_PARAM_H
#define CLIENT_LOGS_PARAM_H

#include <param/param.h>
#include <vmem/vmem_storage.h>

#define CLIENT_STATUS_LOG 1

#define PARAM_MAX_SIZE 512

void client_logs_init(void);
PARAM_DEFINE_STATIC_VMEM(CLIENT_STATUS_LOG, get_upload_log_status, PARAM_TYPE_UINT32, PARAM_MAX_SIZE, 0, PM_READONLY, NULL, NULL, storage, VMEM_UPLOAD_LOG_ADDR, "Latest upload log code");

#endif