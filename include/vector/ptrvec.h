#include <stdbool.h>
#include <stddef.h>
#include "arena/arena.h"

#define DEFAULT_VEC_SIZE 16 

// This can be used as both a table of ptrs,
// and a growable vector of pointers.
typedef struct PointerVector {
    void** items;
    size_t count;
    size_t capacity;
    Arena* arena;
} PointerVector;

bool push_vec_ptr(PointerVector* vec, void* ptr);
void init_vec_ptr(PointerVector* vec, Arena* arena);