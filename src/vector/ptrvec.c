#include <stdalign.h>
#include <stdio.h>
#include <string.h>
#include "vector/ptrvec.h"

bool ensure_capacity_vec_ptr(PointerVector* vec) {
    if (vec->capacity == vec->count) {
        size_t new_capacity = vec->capacity * 2;
        void** new_items = arena_alloc(vec->arena, new_capacity * sizeof(void*), alignof(void*));
        if (!new_items) {
            // TODO: Error handle better.
            fprintf(stderr, "ERROR: Could not allocate new vector list from arena.");
            return false;
        }
        if (vec->count > 0) {
            memcpy(new_items, vec->items, vec->count * sizeof(void*));
        }
        else {
            return false;
        }
        vec->capacity = new_capacity;
        vec->items = new_items;
        return true;
    }
    return true;
}

bool push_vec_ptr(PointerVector* vec, void* ptr) {
    if (ensure_capacity_vec_ptr(vec)) {
        size_t count = vec->count;
        vec->items[count] = ptr;
        vec->count++;
        return true;
    }
    fprintf(stderr, "Failure allocating vector.");
    return false;
}

void init_vec_ptr(PointerVector* vec, Arena* arena) {
    vec->arena = arena;
    vec->capacity = DEFAULT_VEC_SIZE;
    vec->count = 0;
    vec->items = (void**)arena_alloc(arena, sizeof(void*) * vec->capacity, alignof(void*));
}