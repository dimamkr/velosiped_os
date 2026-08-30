#ifndef PAGING_H
#define PAGING_H

#include "types.h"

#define PAGE_SIZE 4096
#define PAGE_DIR_ENTRIES 1024
#define PAGE_TABLE_ENTRIES 1024

void init_paging(void);

#endif