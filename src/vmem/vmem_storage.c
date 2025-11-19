#include <vmem/vmem_file.h>

/* Define file to store persistent params */
VMEM_DEFINE_FILE(storage, "storage", "client_storage.bin.vmem", 10000);