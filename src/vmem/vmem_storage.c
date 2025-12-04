#include <vmem/vmem_file.h>

/* Define file to store persistent params */
VMEM_DEFINE_FILE(client_storage, "client_storage", "client_storage.vmem", 10000);