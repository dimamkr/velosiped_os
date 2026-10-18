#ifndef VFS
#define VFS

#include "types.h"
#include "dynamic_array.h"
#include "hash_table.h"

#include "disk.h"
#include "mbr.h"
#include "fat32.h"
#include "datetime.h"
#include "refcount.h"

#define VFS_FS_FAT32 0x00

#define VFS_FLAG_READ ((uint8_t)1)
#define VFS_FLAG_WRITE ((uint8_t)2)
#define VFS_FLAG_OVERWRITE ((uint8_t)4)
#define VFS_FLAG_APPEND ((uint8_t)8)

#define VFS_ATTRIBUTE_READONLY 0x01
#define VFS_ATTRIBUTE_HIDDEN 0x02
#define VFS_ATTRIBUTE_SYSTEM 0x04
#define VFS_ATTRIBUTE_VOLUME_ID 0x08
#define VFS_ATTRIBUTE_DIRECTORY 0x10
#define VFS_ATTRIBUTE_ARCHIVE 0x20

#define VFS_SEEK_MODE_START 0
#define VFS_SEEK_MODE_END 1
#define VFS_SEEK_MODE_CURRENT 2

typedef struct {
    RC_SHARED_OBJECT_HEADER;

    union {
        fat32_info_t fat32;
    };

    uint8_t disk_id;
    uint8_t partition_id;
    byte_t filesystem;
} vfs_info_t;

typedef struct {
    union {
        fat32_basic_file_info_t fat32;
    };

    char *filename;
    uint32_t size;
    datetime_t creation_datetime;
    datetime_t modify_datetime;
    uint32_t attributes;

    vfs_info_t *info;
} vfs_file_t;

typedef struct {
    vfs_file_t *file;

    uint32_t position;
    byte_t flags;
} vfs_stream_t;

bool_t vfs_update_file_obj(vfs_file_t *file);
void vfs_cleanup_file_obj(vfs_file_t *file);
static void vfs_info_cleanup_routine(void *_info);
bool_t vfs_copy_file_obj(vfs_file_t *dst, const vfs_file_t *src);
void vfs_destroy_files_array(dynamic_array_t *files);
vfs_file_t *vfs_mount_partition(uint8_t disk_id, uint8_t partition_id, char *mountpoint);
bool_t vfs_unmount_partition(uint8_t disk_id, uint8_t partition_id);
dynamic_array_t *vfs_search_mountpoints(const char *pattern, bool_t ignore_case);
void vfs_unmount_partitions_by_name(const char *mountpoint, bool_t ignore_case);
dynamic_array_t *vfs_find_files_in_directory(vfs_file_t *directory, const char *_pattern, bool_t ignore_case);
dynamic_array_t *vfs_find_files(vfs_file_t *directory, const char *_pattern, bool_t ignore_case);
vfs_stream_t *vfs_open(vfs_file_t *file, uint8_t flags);
bool_t vfs_is_filename_allowed(const char *filename);
bool_t vfs_erase(vfs_file_t *file);
bool_t vfs_newfile(vfs_file_t *dir, const char *filename, uint32_t attributes, vfs_file_t *file);
bool_t vfs_newdir(vfs_file_t *dir, const char *dirname, uint32_t attributes, vfs_file_t *new_dir);
bool_t vfs_remove(vfs_file_t *file);
uint32_t vfs_tell(vfs_stream_t *stream);
void vfs_seek(vfs_stream_t *stream, int offset, uint8_t mode);
void vfs_close(vfs_stream_t *stream);
bool_t vfs_read(vfs_stream_t *stream, void *buffer, uint32_t bytes);
bool_t vfs_write(vfs_stream_t *stream, void *buffer, uint32_t bytes);

void vfs_init();

#endif