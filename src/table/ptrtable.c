#include "table/ptrtable.h"
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool zero_set_unused(PtrTable* ptr_table, void** new_items, size_t new_capacity) {
    if (ptr_table->slots >= new_capacity) return true;

    size_t unfilled = (new_capacity - ptr_table->slots) * sizeof(void*);
    void* ptr = &new_items[ptr_table->slots];

    memset(ptr, 0, unfilled);
    return true;
}


bool ensure_slots(PtrTable* ptr_table, size_t n) {
    if (ptr_table->slots < n) {
        // todo: need a way to check how big it actually needs to be.
        // Could be n + 1?
        size_t new_capacity = ptr_table->slots > 0 ? ptr_table->slots : DEFAULT_PTR_TABLE_CAPACITY;
        while (new_capacity < n) new_capacity *= 2;
        void** new_items = arena_alloc(ptr_table->arena, new_capacity * sizeof(void*), alignof(void*));
        if (!new_items) {
            // TODO: Error handle better.
            fprintf(stderr, "ERROR: Could not allocate new table list from arena.");
            return false;
        }
        if (ptr_table->slots != 0) memcpy(new_items, ptr_table->items, ptr_table->slots * sizeof(void*));
        if (!zero_set_unused(ptr_table, new_items, new_capacity)) {
            fprintf(stderr, "ERROR: Failed to null-initialize unused slots.");
            return false;
        }
        ptr_table->slots = new_capacity;
        ptr_table->items = new_items;
        return true;
    }
    return true;
}


bool set(PtrTable* ptr_table, void* ptr, size_t i) {
    if (!ensure_slots(ptr_table, i+1)) return false;

    ptr_table->items[i] = ptr;
    return true;
}

void* get(PtrTable* ptr_table, size_t i) {
    if (i < ptr_table->slots) return ptr_table->items[i];
    return NULL;
}

bool ptr_table_init(PtrTable* ptr_table, Arena* arena) {
    ptr_table->arena = arena;
    ptr_table->items = arena_alloc(arena, DEFAULT_PTR_TABLE_CAPACITY * sizeof(void*), alignof(void*));
    ptr_table->slots = DEFAULT_PTR_TABLE_CAPACITY;
    if (!zero_set_unused(ptr_table, ptr_table->items, DEFAULT_PTR_TABLE_CAPACITY)) {
        fprintf(stderr, "Failed to zero-set new pointer table.");
        return false;
    }
    return true;
}