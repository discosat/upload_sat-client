#ifndef CLIENT_LOGS_PARAM_H
#define CLIENT_LOGS_PARAM_H

#include <param/param.h>
#include <vmem/vmem_storage.h>

// Keep error code for the upload process. 
extern param_t get_upload_log_status;

#define CLIENT_STATUS_LOG 1

#define PARAM_MAX_SIZE 512

PARAM_DEFINE_STATIC_VMEM(CLIENT_STATUS_LOG, get_upload_log_status, PARAM_TYPE_UINT32, PARAM_MAX_SIZE, 0, PM_READONLY, NULL, NULL, storage, VMEM_UPLOAD_LOG_ADDR, "Latest upload log code");

#endif