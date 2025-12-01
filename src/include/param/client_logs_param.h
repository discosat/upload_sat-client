#ifndef CLIENT_LOGS_PARAM_H
#define CLIENT_LOGS_PARAM_H

#include <param/param.h>
#include <vmem/vmem_storage.h>
#include "client_logs_paramids.h"

extern param_t get_upload_log_status;

#define CLIENT_STATUS_LOG 1

void client_logs_param_init(void);
void set_client_log_status(uint32_t status);

#endif