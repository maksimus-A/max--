#include "arena/arena.h"
#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_PTR_TABLE_CAPACITY 16

typedef struct PtrTable {
    void** items;
    size_t slots;
    Arena* arena;
} PtrTable;

bool set(PtrTable* ptr_table, void* ptr, size_t i);
void* get(PtrTable* ptr_table, size_t i);
bool ptr_table_init(PtrTable* ptr_table, Arena* arena);