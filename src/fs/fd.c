#include "fd.h"


static hash_table_t *fd_methods_table = NULL;
static const fd_methods_t *vfs_methods = NULL;


// Внутренние методы для работы с платформами

result_e fd_register_platform(const char *keyword, const fd_methods_t *methods)
{
    if (keyword == NULL)
    {
        vfs_methods = methods;

        return RESULT_OK;
    }
    if (strchr(keyword, ':') != -1)
        return RESULT_INVALID_PARAMETER;

    char *keyword_lower = strdup(keyword);
    string_to_lower(keyword_lower);

    uint32_t size = strlen(keyword) + 1;

    if (hash_table_get(fd_methods_table, (char*)keyword, size) != NULL)
    {
        free(keyword_lower);

        return RESULT_PLATFORM_ALREADY_EXISTS;
    }
        
    hash_table_insert(fd_methods_table, keyword_lower, size, &methods, sizeof(fd_methods_t*));
    free(keyword_lower);

    return RESULT_OK;
}

result_e fd_unregister_platform(const char *keyword)
{
    if (keyword == NULL)
        return RESULT_INVALID_PARAMETER;

    uint32_t size = strlen(keyword) + 1;

    if (!hash_table_erase(fd_methods_table, (char*)keyword, size))
        return RESULT_PLATFORM_NOT_FOUND;

    return RESULT_OK;
}

// Методы для работы с таблицами дескрипторов

fd_info_t *fd_create_info()
{
    fd_info_t *info = malloc(sizeof(fd_info_t));

    info->mex = mex_create();
    info->table = malloc(MEX_BLOCK_BIT_SIZE * sizeof(fd_stream_t));
    info->table_size = MEX_BLOCK_BIT_SIZE;
    info->max_descriptors_count = FD_DEFAULT_MAX_DESCRIPTORS_COUNT;

    return info;
}

void fd_destroy_info(fd_info_t *info)
{
    mex_destroy(info->mex);
    free(info->table);
    free(info);
}

result_e fd_set_max_descriptors_count(fd_info_t *info, uint32_t new_max_descriptors_count)
{
    if (info == NULL)
        return RESULT_INVALID_PARAMETER;

    info->max_descriptors_count = new_max_descriptors_count;
}

result_e fd_open(fd_info_t *info, const char *name, uint8_t flags, uint32_t *descriptor_)
{
    if (info == NULL || name == NULL || descriptor_ == NULL)
        return RESULT_INVALID_PARAMETER;

    const fd_methods_t *methods;
        
    const char *real_name = name;
    char *name_buff = strdup(name);
    uint32_t colon_pos = strchr(name_buff, ':');

    if (colon_pos == -1)
        methods = vfs_methods;
    else
    {
        name_buff[colon_pos] = '\0';
        real_name = name_buff + colon_pos + 1;
        string_to_lower(name_buff);

        methods = hash_table_get(fd_methods_table, name_buff, colon_pos + 1);

        if (methods == NULL)
        {
            free(name_buff);

            return RESULT_PLATFORM_NOT_FOUND;
        }
    }

    uint32_t descriptor = mex_get_mex(info->mex);

    if (descriptor == info->max_descriptors_count || descriptor == -1)
    {
        if (descriptor != -1)
            mex_release(info->mex, descriptor);

        free(name_buff);

        return RESULT_TOO_MANY_DESCRIPTORS;
    }

    fd_platform_specific_object_ptr stream;

    result_e result = methods->open(real_name, flags, &stream);
    free(name_buff);

    if (result != RESULT_OK)
    {
        mex_release(info->mex, descriptor);

        return result;
    }

    if (descriptor >= info->table_size)
    {
        info->table = realloc(info->table, 2 * info->table_size * sizeof(fd_stream_t));
        memset(info->table + info->table_size, '\0', info->table_size * sizeof(fd_stream_t));
        info->table_size *= 2;
    }

    info->table[descriptor]._stream = stream;
    info->table[descriptor].methods = methods;

    *descriptor_ = descriptor;

    return RESULT_OK;
}

result_e fd_close(fd_info_t *info, uint32_t descriptor)
{
    if (descriptor >= info->table_size || info->table[descriptor].methods == NULL)
        return RESULT_INVALID_DESCRIPTOR;

    info->table[descriptor].methods->close(info->table[descriptor]._stream);
    info->table[descriptor].methods = NULL;

    mex_release(info->mex, descriptor);

    return RESULT_OK;
}

// IO

result_e fd_read(fd_info_t *info, uint32_t descriptor, void *buffer, uint32_t buffer_size, uint32_t *read_size)
{
    if (descriptor >= info->table_size || info->table[descriptor].methods == NULL)
        return RESULT_INVALID_DESCRIPTOR;

    return info->table[descriptor].methods->read(info->table[descriptor]._stream, buffer, buffer_size, read_size);
}

result_e fd_write(fd_info_t *info, uint32_t descriptor, void *buffer, uint32_t buffer_size, uint32_t *read_size)
{
    if (descriptor >= info->table_size || info->table[descriptor].methods == NULL)
        return RESULT_INVALID_DESCRIPTOR;

    return info->table[descriptor].methods->write(info->table[descriptor]._stream, buffer, buffer_size, read_size);
}

result_e fd_seek(fd_info_t *info, uint32_t descriptor, int offset, uint8_t mode)
{
    if (descriptor >= info->table_size || info->table[descriptor].methods == NULL)
        return RESULT_INVALID_DESCRIPTOR;

    return info->table[descriptor].methods->seek(info->table[descriptor]._stream, offset, mode);
}

uint32_t fd_tell(fd_info_t *info, uint32_t descriptor)
{
    if (descriptor >= info->table_size || info->table[descriptor].methods == NULL)
        return RESULT_INVALID_DESCRIPTOR;

    return info->table[descriptor].methods->tell(info->table[descriptor]._stream);
}

// INIT

void fd_init()
{
    fd_methods_table = hash_table_create();
}