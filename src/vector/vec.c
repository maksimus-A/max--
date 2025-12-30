#include <stdalign.h>
#include <stdio.h>
#include <string.h>
#include "vector/vec.h"

bool ensure_capacity_vec(Vector* vec) {
    if (vec->capacity == vec->count) {
        size_t new_capacity = vec->capacity * 2;
        void* new_items = arena_alloc(vec->arena, new_capacity * vec->item_size, vec->item_align);
        if (!new_items) {
            // TODO: Error handle better.
            fprintf(stderr, "ERROR: Could not allocate new vector list from arena.");
            return false;
        }
        if (vec->count > 0) {
            memcpy(new_items, vec->items, vec->count * vec->item_size);
        }
        vec->capacity = new_capacity;
        vec->items = new_items;
        return true;
    }
    return true;
}

bool vec_push(Vector* vec, const void* item) {
    if (ensure_capacity_vec(vec)) {
        size_t count = vec->count;
        // memcpy item into vec->items
        char* dst = (char*)(vec->items) + (vec->item_size * count);
        memcpy(dst, item, vec->item_size);
        vec->count++;
        return true;
    }
    fprintf(stderr, "Failure allocating vector.");
    return false;
}

void vec_init(Vector* vec, Arena* arena, size_t item_size, size_t item_align) {
    vec->arena = arena;
    vec->capacity = DEFAULT_VEC_SIZE;
    vec->count = 0;

    vec->item_size = item_size;
    vec->item_align = item_align;
    vec->items = arena_alloc(arena, item_size * vec->capacity, item_align);
}

void vec_clear(Vector* vec) {
    vec->count = 0;
}