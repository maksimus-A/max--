#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "arena/arena.h"
#include "vector/ptrvec.h"

#define VEC_INIT_T(vec, arena, T) \
    vec_init((vec), (arena), sizeof(T), alignof(T))
#define VEC_PUSH_T(vec, value) \
    vec_push((vec), &(value))
// Gives the item in vector at position i.
#define VEC_AT_T(vec, T, i) \
    (((T*)(vec)->items)[(i)])
// Gives a pointer to item in vector at position i.
#define VEC_AT_PTR_T(vec, T, i) \
    (&((T*)(vec)->items)[(i)])
// Pops vector as given type.
#define VEC_POP_AS(vec, Type) (*(Type*)vec_pop(vec))

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
void* vec_pop(Vector* vec);
void vec_init(Vector* vec, Arena* arena, size_t item_size, size_t item_align);