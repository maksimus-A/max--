#pragma once
#include "arena/arena.h"
#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_PTR_TABLE_CAPACITY 16

typedef struct PtrTable {
    void** items;
    size_t slots; // total number allocated; not equal to items actually existing.
    Arena* arena;

    size_t count; // metadata for items actually inserted.
} PtrTable;

bool set_ptr_tbl(PtrTable* ptr_table, void* ptr, size_t i);
void* get_ptr_tbl(PtrTable* ptr_table, size_t i);
bool ptr_table_init(PtrTable* ptr_table, Arena* arena);