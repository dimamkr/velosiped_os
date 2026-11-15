#include "results.h"

static const char* result_table [] = {
    [RESULT_OK] = "Ok",
    [RESULT_INVALID_HANDLE] = "Invalid handle",
    [RESULT_INVALID_FILENAME] = "Invalid filename",
    [RESULT_DISK_ERROR] = "Disk error",
    [RESULT_INVALID_DISK_ID] = "Invalid disk ID",
    [RESULT_INVALID_PARTITION_NUM] = "Invalid partition num",
    [RESULT_NO_FILESYSTEM] = "No filesystem",
    [RESULT_MOUNTPOINT_ALREADY_EXISTS] = "Mountpoint already exists",
    [RESULT_MOUNTPOINT_NOT_FOUND] = "Mountpoint not exists",
    [RESULT_PARTITION_ALREADY_MOUNTED] = "Partition already mounted",
    [RESULT_INVALID_PARAMETER] = "Invalid parameter",
    [RESULT_INVALID_METHOD] = "Invalid method",
    [RESULT_EMPTY_PATH] = "Path is empty",
    [RESULT_FILESYSTEM_MISMATCH] = "Filesystem mismatch",
    [RESULT_FILE_ALREADY_EXISTS] = "File already exists",
    [RESULT_NOT_WRITABLE] = "Stream is not writable",
    [RESULT_NOT_READABLE] = "Stream is not readable",
    [RESULT_BUSY] = "Resource is busy",
    [RESULT_FILE_NOT_FOUND] = "File not found",
    [RESULT_PLATFORM_ALREADY_EXISTS] = "Platform already exists",
    [RESULT_PLATFORM_NOT_FOUND] = "Platform not found",
    [RESULT_TOO_MANY_DESCRIPTORS] = "Too many descriptors",
    [RESULT_INVALID_DESCRIPTOR] = "Invalid descriptor"
};

const char *results_get(result_e result)
{
    return result_table[result];
}