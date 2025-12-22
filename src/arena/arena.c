#include <stdio.h>
#include <stdlib.h>
#include <stdalign.h>
#include <stddef.h>
#include <assert.h>
#include "arena/arena.h"

#define MAX_ALIGN alignof(max_align_t)

size_t max(size_t a, size_t b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

size_t round_up(size_t size, size_t align) {
    /*Gonna write a small example of what's going on here
    because this was confusing me a bit.
    Let's say that base pointer = 1000, used = 22, size=8, align=8.
    We can't do 'store at 1022 (base + used) because 1022 % 8 != 0.
    So we find padding first, and add padding to used.
    In this case, padding = 2, so 1000 + 22 + 2 = 1024 % 8 == 0.
    The aligned is clever bitwise operations to find the padding automatically.
    */
    assert(align && !(align & (align-1))); // assert power of 2
    size_t aligned = (size + (align-1)) & ~(align-1);
    return aligned;
}

int fits_in_current_block(Arena* arena, size_t size, size_t aligned_used) {
    // TODO: add safety checks
    if (arena->curr_block == NULL) {return 0;}
    MemBlock* curr_mem = arena->curr_block;
    if (aligned_used > curr_mem->capacity) { return 0; }
    
    return size <= curr_mem->capacity - aligned_used;
}

MemBlock* create_block(size_t size) {
    // TODO: Check size does not overflow?
    //if (size > SIZE_MAX - sizeof(MemBlock)) return NULL;
    size_t new_block_size = sizeof(MemBlock) + size;
    MemBlock* new_block = malloc(new_block_size);
    if (!new_block) {
        // TODO: Error handle better?
        return NULL;
    }
    new_block->capacity = size;
    new_block->used = 0;
    new_block->next = NULL;

    return new_block;
}

int arena_add_block(Arena* arena, size_t size) {
    MemBlock* new_block = create_block(size);
    if (new_block == NULL) {
        // TODO: Error handle idk
        printf("Block allocation failed.");
        return 0;
    }
    
    if (arena->curr_block) {
        arena->curr_block->next = new_block;
        arena->curr_block = new_block;
        return 1;
    }
    else {
        arena->start_block = new_block;
        arena->curr_block = new_block;
        return 1;
    }
}

size_t choose_block_size(size_t size) {
    // TODO: Actually implement this.
    // I'll handle special cases later. I just
    // wanna try the arena allocator right now.
    return max(DEFAULT_BLOCK_SIZE, size+MAX_ALIGN-1);
}

void bump(Arena* arena, size_t bump_size) {
    // Moves data pointer to correct address
    // (aligned_used + size)
    arena->curr_block->used = bump_size;
}

void* current_ptr(Arena* arena, size_t aligned_used) {
    return arena->curr_block->data + aligned_used;
}

void* arena_alloc(Arena* arena, size_t size, size_t align) {

    if (arena->curr_block == NULL) {
        arena_add_block(arena, choose_block_size(size));
    }

    size_t aligned_used = round_up(arena->curr_block->used, align);
    if (!fits_in_current_block(arena, size, aligned_used)) {
        int success = arena_add_block(arena, choose_block_size(size));
        if (!success) {
            fprintf(stderr, "Failed to add block to arena.");
            return NULL;
        }
        aligned_used = round_up(arena->curr_block->used, align);
    }
    void* p = current_ptr(arena, aligned_used); // answers: where should I place the next block?
    bump(arena, aligned_used+size);
    return p;
}

int arena_init(Arena* arena, size_t size) {
    if (!arena_add_block(arena, choose_block_size(size))) {
        fprintf(stderr, "Failed to initialize arena.");
        return 1;
    }
    return 0;
}

int arena_destroy(Arena* arena) {
    if (!arena || !arena->start_block) {
        fprintf(stderr, "Failed to destroy arena: Arena has no start block.\n");
        return 1;
    }

    MemBlock* block = arena->start_block;
    while (block != NULL) {
        MemBlock* next = block->next;
        free(block);
        block = next;
    }

    arena->start_block = NULL;
    arena->curr_block = NULL;
    return 0;
}

int arena_reset(Arena* arena) {
    if (arena->start_block == NULL) {
        fprintf(stderr, "Failed to reset arena: Arena has no start block.");
        return 1;
    }

    MemBlock* block = arena->start_block;
    while (block->next != NULL) {
        block->used = 0;
        block = block->next;
    }
    block->used = 0;
    arena->curr_block = arena->start_block;
    return 0;
}
