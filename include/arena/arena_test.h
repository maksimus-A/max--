// arena_test.h
// Declarations for arena_tests.c helpers.

#ifndef ARENA_TEST_H
#define ARENA_TEST_H

#include <stddef.h>
#include "arena/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

// Test A: alignment is correct
void test_arena_alignment(Arena* arena);

// Test B: allocations don’t overlap / don’t clobber each other
void test_arena_no_overlap(Arena* arena);

// Test C: block growth works (allocating enough to force new blocks)
void test_arena_block_growth(Arena* arena);

// Extra: reset should rewind and reuse memory as expected
void test_arena_reset_reuse(Arena* arena);

// Convenience: run all tests in a reasonable order
void test_arena_all(Arena* arena);

void arena_print_layout(const Arena* arena);

static void hexdump_lines(const void* p, size_t n);

#ifdef __cplusplus
}
#endif

#endif // ARENA_TEST_H
