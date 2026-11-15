#ifndef FD
#define FD

#include "types.h"
#include "mex.h"
#include "refcount.h"
#include "hash_table.h"
#include "string.h"
#include "system.h"
#include "results.h"

#define FD_DEFAULT_MAX_DESCRIPTORS_COUNT 1024

typedef void* fd_platform_specific_object_ptr;

typedef struct {
    result_e (*open) (const char *name, uint8_t flags, fd_platform_specific_object_ptr *stream);
    void (*close) (fd_platform_specific_object_ptr stream);
    result_e (*read) (fd_platform_specific_object_ptr stream, void *buffer, uint32_t buffer_size, uint32_t *read_size);
    result_e (*write) (fd_platform_specific_object_ptr stream, void *buffer, uint32_t buffer_size, uint32_t *write_size);
    result_e (*seek) (fd_platform_specific_object_ptr stream, int offset, uint8_t mode);
    uint32_t (*tell) (fd_platform_specific_object_ptr stream);
} fd_methods_t;

typedef struct {
    const fd_methods_t *methods;
    fd_platform_specific_object_ptr _stream;
} fd_stream_t;

typedef struct {
    mex_t *mex;
    fd_stream_t *table;
    uint32_t table_size;
    uint32_t max_descriptors_count;
} fd_info_t;


result_e fd_register_platform(const char *keyword, const fd_methods_t *methods);
result_e fd_unregister_platform(const char *keyword);

fd_info_t *fd_create_info();
void fd_destroy_info(fd_info_t *info);
result_e fd_set_max_descriptors_count(fd_info_t *info, uint32_t new_max_descriptors_count);
result_e fd_open(fd_info_t *info, const char *name, uint8_t flags, uint32_t *descriptor_);
result_e fd_close(fd_info_t *info, uint32_t descriptor);

result_e fd_read(fd_info_t *info, uint32_t descriptor, void *buffer, uint32_t buffer_size, uint32_t *read_size);
result_e fd_write(fd_info_t *info, uint32_t descriptor, void *buffer, uint32_t buffer_size, uint32_t *read_size);
result_e fd_seek(fd_info_t *info, uint32_t descriptor, int offset, uint8_t mode);
uint32_t fd_tell(fd_info_t *info, uint32_t descriptor);

void fd_init();

#endif