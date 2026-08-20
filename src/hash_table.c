#include "hash_table.h"

uint32_t primes[32] = {0, 2, 5, 11, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209,
                       16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 4194319, 8388617,
                       16777259, 33554467, 67108879, 134217757, 268435459, 536870923, 1073741827, 2147483659};

static inline uint32_t get_hash(void *ptr, uint32_t element_size, uint32_t buff_count)
{
    uint32_t hash = 0;
    const uint8_t *bytes = (const uint8_t *)ptr;
    for (uint32_t i = 0; i < element_size; i++)
    {
        hash = hash * 1073741827 + bytes[i];
    }
    return hash % buff_count;
}

static inline void init_buff(linked_list_node_t ***buff, uint32_t buff_count_index_new)
{
    uint32_t size = primes[buff_count_index_new] * sizeof(linked_list_node_t **);
    *buff = malloc(size);
    memset(*buff, 0, size);
}

static inline void destroy_buff(hash_table_t *hash_table)
{
    for (uint32_t i = 0; i < primes[hash_table->buff_count_index]; ++i)
    {
        linked_list_destroy(hash_table->key_buff_root + i);
        linked_list_destroy(hash_table->val_buff_root + i);
    }

    free(hash_table->key_buff_root);
    free(hash_table->val_buff_root);

    hash_table->buff_count_index = 0;
}

static inline void hash_table_rebuild(hash_table_t *hash_table, uint32_t buff_count_index_new)
{
    linked_list_node_t **old_key_buff_root = hash_table->key_buff_root;
    linked_list_node_t **old_val_buff_root = hash_table->val_buff_root;
    uint32_t old_buff_count_index = hash_table->buff_count_index;

    init_buff(&hash_table->key_buff_root, buff_count_index_new);
    init_buff(&hash_table->val_buff_root, buff_count_index_new);

    hash_table->buff_count_index = buff_count_index_new;
    hash_table->elements_count = 0;

    for (uint32_t i = 0; i < primes[old_buff_count_index]; ++i)
    {
        linked_list_node_t *key_node = old_key_buff_root[i];
        linked_list_node_t *vall_node = old_val_buff_root[i];

        while (key_node)
        {
            hash_table_insert(hash_table, key_node->value, key_node->value_size, vall_node->value, vall_node->value_size);
            key_node = key_node->right;
            vall_node = vall_node->right;
        }

        linked_list_destroy(old_key_buff_root + i);
        linked_list_destroy(old_val_buff_root + i);
    }

    free(old_key_buff_root);
    free(old_val_buff_root);
}

hash_table_t *hash_table_create()
{
    hash_table_t *t = (hash_table_t *)malloc(sizeof(hash_table_t));
    t->elements_count = 0;

    init_buff(&t->key_buff_root, 1);
    init_buff(&t->val_buff_root, 1);
    t->buff_count_index = 1;

    return t;
}

void hash_table_destroy(hash_table_t *hash_table)
{
    destroy_buff(hash_table);
    free(hash_table);
}

// TODO не вызывать erase а удалять сразу в нужном месте
void hash_table_insert(hash_table_t *hash_table, void *key, uint32_t key_size, void *value, uint32_t value_size)
{
    hash_table_erase(hash_table, key, key_size);

    uint32_t hash = get_hash(key, key_size, primes[hash_table->buff_count_index]);

    linked_list_add_begin(hash_table->key_buff_root + hash, key, key_size);
    linked_list_add_begin(hash_table->val_buff_root + hash, value, value_size);

    hash_table->elements_count++;
    if (hash_table->elements_count >= primes[hash_table->buff_count_index] / 2)
    {
        hash_table_rebuild(hash_table, hash_table->buff_count_index + 1);
    }
}

// результат это есть ли там такой элемент
bool hash_table_erase(hash_table_t *hash_table, void *key, uint32_t key_size)
{
    uint32_t hash = get_hash(key, key_size, primes[hash_table->buff_count_index]);
    linked_list_node_t *key_node = hash_table->key_buff_root[hash];
    linked_list_node_t *val_node = hash_table->val_buff_root[hash];

    while (key_node)
    {
        if (key_size == key_node->value_size && memcmp(key, key_node->value, key_size))
        {
            linked_list_erase(hash_table->key_buff_root + hash, key_node);
            linked_list_erase(hash_table->val_buff_root + hash, val_node);

            hash_table->elements_count--;

            if (hash_table->buff_count_index > 2 && hash_table->elements_count <= primes[hash_table->buff_count_index - 2])
            {
                hash_table_rebuild(hash_table, hash_table->buff_count_index - 2);
            }

            return true;
        }

        key_node = key_node->right;
        val_node = val_node->right;
    }

    return false;
}

void *hash_table_get(hash_table_t *hash_table, void *key, uint32_t key_size)
{
    uint32_t hash = get_hash(key, key_size, primes[hash_table->buff_count_index]);
    linked_list_node_t *key_node = hash_table->key_buff_root[hash];
    linked_list_node_t *val_node = hash_table->val_buff_root[hash];

    while (key_node)
    {
        if (key_size == key_node->value_size && memcmp(key, key_node->value, key_size))
        {
            return val_node->value;
        }

        key_node = key_node->right;
        val_node = val_node->right;
    }

    return NULL;
}

void hash_table_reserve(hash_table_t *hash_table, uint32_t buff_count)
{
    // TODO бинпоиском
    for (int i = 0; i < 32; ++i)
    {
        if (primes[i] >= buff_count)
        {
            hash_table_rebuild(hash_table, i);
            return;
        }
    }
}