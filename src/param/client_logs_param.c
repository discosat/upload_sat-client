#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>

#include "client_logs_param.h"

static uint32_t _get_upload_log_status = 0;

PARAM_DEFINE_REMOTE(1, remote_upload_log_status, 170, PARAM_TYPE_UINT32, -1, 0, PM_READONLY, &_get_upload_log_status, "Remote status from Node 170");

int INDEX_ALL = -1; /* Pull/push all indices */
int VERBOSE = 0;    /* Do not print additional debug output */
int TIMEOUT = 1000; /* Timeout for remote access [ms] */
int VERSION = 2;    /* Current param interface version */

// Registration function
void client_logs_param_init(void)
{
    printf("\t%s - [DEBUG] accessing remote client status log %s\n", "\x1B[33m", "\x1B[0m");
    if (param_pull_single(&remote_upload_log_status, INDEX_ALL, 1, VERBOSE, SERVER_ADDR, TIMEOUT, 2) != 0)
    {
        printf("\t%s - [ERROR] Retrieving parameter value failed! %s\n", "\x1B[31m", "\x1B[0m");
    }
}

// Helper to set value
void set_client_log_status(uint32_t status)
{
    printf("\t%s - [DEBUG] setting client status log %s\n", "\x1B[33m", "\x1B[0m");
    _get_upload_log_status = status;
}

// call in main loop or task to update the cache
int fetch_server_status(void)
{
    printf("Pulling status from Node 170...\n");

    int res = param_pull_single(&remote_upload_log_status, INDEX_ALL, 1, VERBOSE, SERVER_ADDR, TIMEOUT, 2);

    if (res < 0)
    {
        printf("\t%s - [ERROR] Retrieving parameter value failed (Error: %d) %s\n", "\x1B[31m", res, "\x1B[0m");
        return -1;
    }

    printf("Success!!\n");
    return 0;
}