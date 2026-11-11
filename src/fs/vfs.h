#ifndef VFS
#define VFS

#include "types.h"
#include "dynamic_array.h"
#include "hash_table.h"
#include "disk.h"
#include "mbr.h"
#include "datetime.h"
#include "refcount.h"
#include "string.h"

// константы

#define VFS_ITERATOR_DEFAULT_QUANT_SIZE 128
#define VFS_DEFAULT_UMOUNT_THRESHOLD 2
#define VFS_DIRECTORY_READ_ALL 0xFFFFFFFF

// флаги открытия файлов

#define VFS_FLAG_READ ((uint8_t)1)
#define VFS_FLAG_WRITE ((uint8_t)2)
#define VFS_FLAG_OVERWRITE ((uint8_t)4)
#define VFS_FLAG_APPEND ((uint8_t)8)

// аттрибуты файлов

#define VFS_ATTRIBUTE_READONLY 0x01
#define VFS_ATTRIBUTE_HIDDEN 0x02
#define VFS_ATTRIBUTE_SYSTEM 0x04
#define VFS_ATTRIBUTE_VOLUME_ID 0x08
#define VFS_ATTRIBUTE_DIRECTORY 0x10
#define VFS_ATTRIBUTE_ARCHIVE 0x20

// режимы vfs_seek

#define VFS_SEEK_MODE_START 0
#define VFS_SEEK_MODE_END 1
#define VFS_SEEK_MODE_CURRENT 2

// перечисление результатов функций

typedef enum {
    VFS_RESULT_OK,
    VFS_RESULT_INVALID_HANDLE,
    VFS_RESULT_INVALID_FILENAME,
    VFS_RESULT_DISK_ERROR,
    VFS_RESULT_INVALID_DISK_ID,
    VFS_RESULT_INVALID_PARTITION_NUM,
    VFS_RESULT_NO_FILESYSTEM,
    VFS_RESULT_MOUNTPOINT_ALREADY_EXISTS,
    VFS_RESULT_MOUNTPOINT_NOT_EXISTS,
    VFS_RESULT_PARTITION_ALREADY_MOUNTED,
    VFS_RESULT_INVALID_PARAMETER,
    VFS_RESULT_INVALID_METHOD,
    VFS_RESULT_EMPTY_PATH,
    VFS_RESULT_FILESYSTEM_MISMATCH,
    VFS_RESULT_FILE_ALREADY_EXISTS,
    VFS_RESULT_NOT_WRITABLE,
    VFS_RESULT_NOT_READABLE,
    VFS_RESULT_FILESYSTEM_IS_BUSY,
    VFS_RESULT_FILE_NOT_FOUND
} vfs_result_e;


// унификация платформенно-специфичного функционала

typedef void* vfs_platform_specific_file_ptr;
typedef void* vfs_platform_specific_info_ptr;
typedef void* vfs_platform_specific_directory_reader_ptr;
typedef void* vfs_platform_specific_stream_ptr;

typedef struct {
    char *filename;
    uint32_t attributes;
    datetime_t creation_datetime;
    datetime_t modify_datetime;
    uint32_t size;
} vfs_file_header_t;

typedef struct {
    uint32_t filesystem_id;
    uint8_t disk_id;
    uint8_t partition_num;
} vfs_info_header_t;

typedef struct {
    // методы для создания/освобождения общей информации о файловой системе
    vfs_platform_specific_info_ptr (*allocate_info_object) ();
    void (*release_info_object) (vfs_platform_specific_info_ptr info);

    // методы для работы с объектом файловой системы
    vfs_result_e (*get_info) (uint8_t disk_id, uint8_t partition_num, vfs_platform_specific_info_ptr *result);
    vfs_result_e (*mount) (vfs_platform_specific_info_ptr info, const char *mountpoint, vfs_platform_specific_file_ptr *result);
    vfs_result_e (*get_info_header) (vfs_platform_specific_info_ptr info, vfs_info_header_t *header);

    // методы для создания/освобождения встроенных объектов файла
    vfs_platform_specific_file_ptr (*allocate_file_object) ();
    vfs_platform_specific_file_ptr (*copy_file_object) (vfs_platform_specific_file_ptr *obj);
    void (*release_file_object) (vfs_platform_specific_file_ptr *obj);

    // методы для работы с папками
    vfs_result_e (*create_directory) (vfs_platform_specific_info_ptr info, vfs_platform_specific_file_ptr dir, const char *dirname, uint32_t attributes, vfs_platform_specific_file_ptr *result);
    vfs_result_e (*open_directory) (vfs_platform_specific_info_ptr info, vfs_platform_specific_file_ptr dir, vfs_platform_specific_directory_reader_ptr *result, uint32_t iterator_quant_size);
    vfs_result_e (*read_directory) (vfs_platform_specific_directory_reader_ptr dir_reader, vfs_platform_specific_file_ptr *buffer, uint32_t buffer_size, uint32_t *read_size);
    void (*close_directory) (vfs_platform_specific_directory_reader_ptr dir_reader);
    vfs_result_e (*get_file_by_name) (vfs_platform_specific_info_ptr info, vfs_platform_specific_file_ptr dir, const char *filename, vfs_platform_specific_file_ptr *result);

    // методы для работы с файлами
    vfs_result_e (*create_file) (vfs_platform_specific_info_ptr info, vfs_platform_specific_file_ptr dir, const char *filename, uint32_t attributes, vfs_platform_specific_file_ptr *result);
    vfs_result_e (*set_file_parameters) (vfs_platform_specific_info_ptr info, vfs_platform_specific_file_ptr file, vfs_file_header_t *parameters);
    vfs_result_e (*get_file_parameters) (vfs_platform_specific_info_ptr info, vfs_platform_specific_file_ptr file, vfs_file_header_t *parameters);
    vfs_result_e (*open_file) (vfs_platform_specific_info_ptr info, vfs_platform_specific_file_ptr file, uint8_t flags, vfs_platform_specific_stream_ptr *result);
    vfs_result_e (*erase_file) (vfs_platform_specific_info_ptr info, vfs_platform_specific_file_ptr file);
    vfs_result_e (*remove_file) (vfs_platform_specific_info_ptr info, vfs_platform_specific_file_ptr file);
    vfs_result_e (*seek) (vfs_platform_specific_stream_ptr stream, int offset, uint8_t mode);
    uint32_t (*tell) (vfs_platform_specific_stream_ptr stream);
    vfs_result_e (*write_file) (vfs_platform_specific_stream_ptr stream, void *buffer, uint32_t buffer_size, uint32_t *bytes_wrote);
    vfs_result_e (*read_file) (vfs_platform_specific_stream_ptr stream, void *buffer, uint32_t buffer_size, uint32_t *bytes_read);
    void (*close_file) (vfs_platform_specific_stream_ptr stream);

} vfs_platform_specific_methods_t;

typedef struct {
    RC_SHARED_OBJECT_HEADER;

    vfs_platform_specific_info_ptr _info;

    const vfs_platform_specific_methods_t *methods;
    vfs_info_header_t header;
} vfs_info_t;

typedef struct {
    vfs_platform_specific_file_ptr _file;

    vfs_info_t *info;
    vfs_file_header_t header;
} vfs_file_t;

typedef struct {
    vfs_platform_specific_directory_reader_ptr _reader;

    vfs_file_t *directory;
    char *pattern;
    bool_t ignore_case;

    vfs_platform_specific_file_ptr *retention_buffer;
    uint32_t retention_buffer_size;
    uint32_t retention_buffer_index;
} vfs_directory_reader_t;

typedef struct {
    vfs_directory_reader_t **layers_stack;

    dynamic_array_t *roots;

    char *pattern;
    char **subpatterns;
    uint32_t path_depth;
    bool_t ignore_case;
} vfs_tree_searcher_t;

typedef struct {
    vfs_platform_specific_stream_ptr _stream;

    vfs_file_t *file;
    byte_t flags;
} vfs_stream_t;

// заголовки функций

const char *vfs_get_result(vfs_result_e result);
void vfs_cleanup_file_obj(vfs_file_t *file);
static void vfs_info_cleanup_routine(void *_info);
vfs_result_e vfs_copy_file_obj(vfs_file_t *dst, const vfs_file_t *src);
void vfs_destroy_files_array(dynamic_array_t *files);
vfs_result_e vfs_mount_partition(uint8_t disk_id, uint8_t partition_num, char *mountpoint);
vfs_result_e vfs_unmount_partition(uint8_t disk_id, uint8_t partition_id, uint32_t threshold);
dynamic_array_t *vfs_search_mountpoints(const char *pattern, bool_t ignore_case);
void vfs_unmount_partitions_by_name(const char *mountpoint, bool_t ignore_case);
vfs_result_e vfs_open_directory(vfs_file_t *directory, uint32_t iterator_quant_size, const char *pattern, bool_t ignore_case, vfs_directory_reader_t **_result);
vfs_result_e vfs_read_directory(vfs_directory_reader_t *reader, vfs_file_t *buffer, uint32_t buffer_size, uint32_t *read_size);
void vfs_close_directory(vfs_directory_reader_t *reader);
vfs_result_e vfs_create_tree_searcher(vfs_file_t *directory, const char *pattern, bool_t ignore_case, vfs_tree_searcher_t **_searcher);
vfs_result_e vfs_search(vfs_tree_searcher_t *searcher, vfs_file_t *buffer, uint32_t buffer_size, uint32_t *read_size);
void vfs_destroy_tree_searcher(vfs_tree_searcher_t *searcher);
vfs_result_e vfs_get_file_by_path(vfs_file_t *current_dir, const char *path, vfs_file_t **file);
vfs_result_e vfs_open(vfs_file_t *file, uint8_t flags, vfs_stream_t **_stream);
bool_t vfs_is_filename_allowed(const char *filename);
vfs_result_e vfs_erase(vfs_file_t *file);
vfs_result_e vfs_newfile(vfs_file_t *dir, const char *filename, uint32_t attributes, vfs_file_t **_file);
vfs_result_e vfs_newdir(vfs_file_t *dir, const char *filename, uint32_t attributes, vfs_file_t **_new_dir);
vfs_result_e vfs_remove(vfs_file_t *file);
uint32_t vfs_tell(vfs_stream_t *stream);
vfs_result_e vfs_seek(vfs_stream_t *stream, int offset, uint8_t mode);
vfs_result_e vfs_read(vfs_stream_t *stream, void *buffer, uint32_t bytes, uint32_t *bytes_read);
vfs_result_e vfs_write(vfs_stream_t *stream, void *buffer, uint32_t bytes, uint32_t *bytes_wrote);
void vfs_close(vfs_stream_t *stream);

const vfs_platform_specific_methods_t *vfs_detect_platform(uint8_t disk_id, uint8_t partition_num);
void vfs_register_platform(uint8_t type, const vfs_platform_specific_methods_t *methods);
void vfs_init();

#endif