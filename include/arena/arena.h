/* An arena/bump allocator is more efficient with memory and memory freeing.
I'm doing it because I want to learn more about C, as well as for performance.*/
#include <stddef.h>
typedef struct MemBlock MemBlock;
struct MemBlock {
    struct MemBlock* next;
    size_t capacity;
    size_t used;
    unsigned char data[];
    /* Writing thsi comment for myself because this is new to me:
    I use 'unsigned char' to tell the compiler it's just a big bag of bytes that I'm managing.
    char is the only type that allows aliasing to arbitrary object representations (and is 1 byte).
    The arena (should) return a void pointer, and I'll cast it to the type I need.
    TODO: I need to also align the bytes (add padding) so memory knows what's happening.
    Ex. my astnode is 12 bytes, but alignment requires 16, I add 4 bytes.*/
};

typedef struct Arena {
    MemBlock* start_block;
    MemBlock* curr_block;
} Arena;

void* arena_alloc(Arena* arena, size_t size, size_t align);