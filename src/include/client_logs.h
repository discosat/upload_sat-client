#ifndef CLIENT_LOGS_H
#define CLIENT_LOGS_H

#include <stdlib.h>
#include <stdint.h>

/* Error domain codes */
typedef enum CLIENT_LOG_CODE
{
    UPLOAD_SUCCESS = 0,

    // General error
    ERR_UNKNOWN_ERR = 100,

    // File destination
    ERR_BAD_FILE_DEST = 200,

    // DTP/Protobuf related
    ERR_PROTOBUF_UNPACK_FAILURE = 300,
    ERR_DTP_OPT_MEM_ALL = 301,
    ERR_DTP_THREAD_CREATION = 302,

    // CMD uploading
    ERR_OPEN_FILE = 400,
    ERR_AUTH_FAILED = 401,
    ERR_FILE_TOO_SHORT = 402,
    ERR_TMP_FILE_CREATION = 403,
    ERR_CHMOD_FILE = 404,
    
} CLIENT_LOG_CODE;

/**
 * Set the uploader log parameter to the specified log code value
 * @param log_code Code of the log
*/
void set_log_param(CLIENT_LOG_CODE log_code);

/**
 * Dummy function for init
 */
void upload_logs_init(param_t *param) 

#endif