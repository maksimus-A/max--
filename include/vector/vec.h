#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "arena/arena.h"
#include "vector/ptrvec.h"

// This can be used as both a table of ptrs,
// and a growable vector of pointers.
typedef struct Vector {
    void* items;
    size_t count;
    size_t capacity;

    // Size/alignment of item
    size_t item_size;
    size_t item_align;
    Arena* arena;
} Vector;

bool vec_push(Vector* vec, const void* item);
void vec_init(Vector* vec, Arena* arena, size_t item_size, size_t item_align);