#ifndef CLIENT_LOGS_PARAM_H
#define CLIENT_LOGS_PARAM_H

#include <param/param.h>
#include <vmem/vmem_storage.h>
#include "client_logs_paramids.h"

extern param_t remote_upload_log_status;

#define SERVER_ADDR 170

// Should match STATUS_LOG from the upload client
#define CLIENT_STATUS_LOG 1

void client_logs_param_init(void);
void set_client_log_status(uint32_t status);
int fetch_server_status(void);

#endif