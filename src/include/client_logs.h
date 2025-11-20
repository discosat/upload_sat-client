#ifndef CLIENT_LOGS_H
#define CLIENT_LOGS_H

#include <stdlib.h>
#include <stdint.h>

/* Error domain codes */
typedef enum CLIENT_LOG_CODE
{
    UPLOAD_SUCCESS = 100,

    ERR_UNKNOWN_ERR = 200,
    ERR_BAD_FILE_DEST = 201,

    ERR_PROTOBUF_UNPACK_FAILURE = 300,
    ERR_DTP_OPT_MEM_ALL = 301,
    ERR_DTP_THREAD_CREATION = 302,
    
} CLIENT_LOG_CODE;

/**
 * Set the uploader log parameter to the specified log code value
 * @param log_code Code of the log
*/
void set_log_param(CLIENT_LOG_CODE log_code);

/**
 * Dummy function
 */
void client_logs_init(void);

#endif