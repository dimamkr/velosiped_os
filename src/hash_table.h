#ifndef HASH_SET
#define HASH_SET

#include "types.h"
#include "linked_list.h"
#include "system.h"

typedef struct
{
    uint32_t elements_count;
    uint32_t buff_count_index;
    linked_list_node_t **key_buff_root;
    linked_list_node_t **val_buff_root;
} hash_table_t;

hash_table_t *hash_table_create();

void hash_table_destroy(hash_table_t *hash_table);

void hash_table_insert(hash_table_t *hash_table, void *key, uint32_t key_size, void *value, uint32_t value_size);

bool hash_table_erase(hash_table_t *hash_table, void *key, uint32_t key_size);

void *hash_table_get(hash_table_t *hash_table, void *key, uint32_t key_size);

// резервирует в количестве первое число из primes не меньше данного
void hash_table_reserve(hash_table_t *hash_table, uint32_t buff_count);

#endif
