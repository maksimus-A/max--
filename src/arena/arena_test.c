/* GENERATED WITH CHATGPT! I WAS TOO LAZY TO WRITE TESTS! */
// arena_tests.c
// Quick, pullable test functions for your arena allocator.
//
// Usage (example main):
//   #include "arena/arena.h"
//   int main(void) {
//       Arena a = {0};
//       arena_init(&a, 0);
//
//       test_arena_alignment(&a);
//       test_arena_no_overlap(&a);
//       test_arena_block_growth(&a);
//       test_arena_reset_reuse(&a);
//
//       arena_destroy(&a);
//       return 0;
//   }
//
// Suggested build flags (clang/gcc):
//   -g -O0 -Wall -Wextra -Wpedantic
//   -fsanitize=address,undefined -fno-omit-frame-pointer
//
// Notes:
// - These tests assume your arena_alloc expects power-of-two alignments.
// - These tests touch Arena internals (curr_block/start_block) on purpose.
/* Forward declarations (needed because ARENA_DUMP uses these) */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdalign.h>
#include <assert.h>

#include "arena/arena.h"


static void arena_print_layout(const Arena* arena);
static void hexdump_lines(const void* p, size_t n);
#if defined(MAXC_ARENA_DUMP) && MAXC_ARENA_DUMP
#  define ARENA_DUMP(tag, arena_ptr) do { \
        printf("=== ARENA DUMP: %s ===\n", (tag)); \
        arena_print_layout((arena_ptr)); \
    } while (0)
#else
#  define ARENA_DUMP(tag, arena_ptr) do { (void)(tag); (void)(arena_ptr); } while (0)
#endif

/* ---- configurable dump limits ---- */
#ifndef ARENA_DUMP_MAX_BYTES_PER_BLOCK
#define ARENA_DUMP_MAX_BYTES_PER_BLOCK 64
#endif



#ifndef TEST_ASSERT
#define TEST_ASSERT(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        assert(expr); \
    } \
} while (0)
#endif

static int is_aligned_to(const void* p, size_t align) {
    // align must be power of 2
    return (((uintptr_t)p) & (align - 1)) == 0;
}

static void fill_bytes(void* p, size_t n, unsigned char value) {
    memset(p, value, n);
}

static void assert_all_bytes(const void* p, size_t n, unsigned char value) {
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++) {
        if (b[i] != value) {
            fprintf(stderr,
                    "[FAIL] byte mismatch at i=%zu: got=0x%02X expected=0x%02X\n",
                    i, (unsigned)b[i], (unsigned)value);
            TEST_ASSERT(0);
        }
    }
}

static int ranges_overlap(const unsigned char* a, size_t asz,
                          const unsigned char* b, size_t bsz) {
    // [a, a+asz) overlaps [b, b+bsz) ?
    return (a < b + bsz) && (b < a + asz);
}

/* ------------------------------
   Test A: alignment is correct
   ------------------------------ */
void test_arena_alignment(Arena* arena) {
    printf("[TEST] alignment\n");

    // Mix some sizes/alignments to force padding behavior.
    // We intentionally allocate in an order that will create misaligned "used"
    // before requesting a larger alignment.
    void* p1 = arena_alloc(arena, sizeof(char), alignof(char));      // align 1
    void* p2 = arena_alloc(arena, sizeof(int), alignof(int));        // align 4
    void* p3 = arena_alloc(arena, sizeof(double), alignof(double));  // align typically 8
    void* p4 = arena_alloc(arena, sizeof(void*), alignof(void*));    // align pointer

    TEST_ASSERT(p1 && p2 && p3 && p4);

    TEST_ASSERT(is_aligned_to(p1, alignof(char)));
    TEST_ASSERT(is_aligned_to(p2, alignof(int)));
    TEST_ASSERT(is_aligned_to(p3, alignof(double)));
    TEST_ASSERT(is_aligned_to(p4, alignof(void*)));

    // Optional: a struct with higher alignment
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
typedef struct Aligned16 {
    long double force_align; // typically 16-byte aligned
    char x[16];
} Aligned16;
#endif

    printf("[PASS] alignment\n");
}

/* --------------------------------
   Test B: allocations don’t overlap
   -------------------------------- */
void test_arena_no_overlap(Arena* arena) {
    printf("[TEST] no-overlap\n");

    const size_t N = 32;

    unsigned char* a = (unsigned char*)arena_alloc(arena, N, alignof(unsigned char));
    unsigned char* b = (unsigned char*)arena_alloc(arena, N, alignof(unsigned char));

    TEST_ASSERT(a && b);

    // If the ranges overlap, patterns will collide or this will explicitly detect it.
    TEST_ASSERT(!ranges_overlap(a, N, b, N));

    fill_bytes(a, N, 0xAA);
    fill_bytes(b, N, 0xBB);

    // Ensure b didn't clobber a
    assert_all_bytes(a, N, 0xAA);
    assert_all_bytes(b, N, 0xBB);

    printf("[PASS] no-overlap\n");
}

/* -------------------------------
   Test C: block growth works
   ------------------------------- */
void test_arena_block_growth(Arena* arena) {
    printf("[TEST] block-growth\n");

    // We’ll allocate enough to force arena_add_block.
    // We detect growth by seeing curr_block change from start_block.
    MemBlock* start = arena->start_block;
    TEST_ASSERT(start != NULL);

    // Keep a pointer from the first block and make sure it stays valid after growth.
    unsigned char* sentinel = (unsigned char*)arena_alloc(arena, 64, alignof(max_align_t));
    TEST_ASSERT(sentinel);
    fill_bytes(sentinel, 64, 0xCC);

    // Allocate in medium chunks until we see a new block.
    // Chunk size: 1024 tends to trigger growth quickly without being too huge.
    const size_t chunk = 1024;
    const size_t iters = 20000; // upper bound; should break much earlier

    MemBlock* before = arena->curr_block;
    TEST_ASSERT(before != NULL);

    MemBlock* grew_to = NULL;
    for (size_t i = 0; i < iters; i++) {
        void* p = arena_alloc(arena, chunk, alignof(max_align_t));
        TEST_ASSERT(p);

        if (arena->curr_block != start) {
            grew_to = arena->curr_block;
            break;
        }
    }

    TEST_ASSERT(grew_to != NULL);           // must have grown to a new block
    TEST_ASSERT(arena->curr_block != NULL); // still valid

    // Ensure earlier memory is still intact.
    assert_all_bytes(sentinel, 64, 0xCC);

    printf("[PASS] block-growth (start=%p curr=%p)\n", (void*)start, (void*)arena->curr_block);
}

/* -------------------------------------
   Extra: reset reuses memory as expected
   ------------------------------------- */
void test_arena_reset_reuse(Arena* arena) {
    printf("[TEST] reset-reuse\n");

    // Allocate something and remember pointer; then reset and allocate same shape.
    void* p1 = arena_alloc(arena, 128, alignof(max_align_t));
    TEST_ASSERT(p1);
    fill_bytes(p1, 128, 0x5A);

    // Move forward a bit
    void* p2 = arena_alloc(arena, 64, alignof(max_align_t));
    TEST_ASSERT(p2);
    (void)p2;

    // Reset should put curr_block back to start and used=0 everywhere.
    TEST_ASSERT(arena_reset(arena) == 0);

    // printf("Starting used & curr block used: %d %d\n", (int)arena->start_block->used, (int)arena->curr_block->used);
    // printf("start block address: %p\n", (void*)&arena->start_block);
    // printf("start block capacity: %d\n", (int)arena->start_block->capacity);

    void* p1_again = arena_alloc(arena, 128, alignof(max_align_t));
    // printf("p1 and p1_again: %p %p\n", p1, p1_again);
    // printf("Start block & curr block: %p %p\n", (void*)arena->start_block, (void*)arena->curr_block);
    TEST_ASSERT(p1_again);

    // If reset truly rewound, the first allocation after reset should come from the start again.
    // In a typical arena, this ends up being the same address as the first allocation.
    TEST_ASSERT(p1_again == p1);

    printf("[PASS] reset-reuse\n");
}

/* -------------------------------------
   Optional: a tiny "smoke test" wrapper
   ------------------------------------- */
void test_arena_all(Arena* arena) {
    ARENA_DUMP("before arena alignment", arena);
    test_arena_alignment(arena);
    ARENA_DUMP("after arena alignment", arena);

    ARENA_DUMP("before arena no overlap", arena);
    test_arena_no_overlap(arena);
    ARENA_DUMP("after arena no overlap", arena);

    ARENA_DUMP("before block growth", arena);
    test_arena_block_growth(arena);
    ARENA_DUMP("after block growth", arena);

    // Since this test is failing due to previous test creating a new curr_block 
    arena_destroy(arena);
    Arena a = {0};
    arena_init(&a, 256);
    ARENA_DUMP("before arena reset-reuse", arena);
    test_arena_reset_reuse(&a);
    ARENA_DUMP("after arena reset-reuse", arena);
}

/* View memory as hexdump (with ASCII gutter) */
static void hexdump_lines(const void* p, size_t n) {
    const unsigned char* b = (const unsigned char*)p;

    for (size_t i = 0; i < n; i += 16) {
        size_t line_n = (n - i < 16) ? (n - i) : 16;

        // Address
        printf("    %p: ", (const void*)(b + i));

        // Hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (j < line_n) printf("%02X ", b[i + j]);
            else printf("   ");
        }

        // ASCII (printable bytes)
        printf(" |");
        for (size_t j = 0; j < line_n; j++) {
            unsigned char c = b[i + j];
            printf("%c", (c >= 32 && c <= 126) ? (char)c : '.');
        }
        printf("|\n");
    }
}

/* Print arena memory layout + a hexdump of block data */
void arena_print_layout(const Arena* arena) {
    if (!arena) {
        printf("Arena: (null)\n");
        return;
    }
    if (!arena->start_block) {
        printf("Arena: (empty) start_block=NULL\n");
        return;
    }

    const MemBlock* b = arena->start_block;
    int i = 0;
    while (b) {
        size_t used = b->used;
        size_t cap  = b->capacity;

        printf("Block %d: %p  used=%zu  cap=%zu  data=%p  next=%p\n",
               i, (void*)b, used, cap, (void*)b->data, (void*)b->next);

        // Decide how many bytes to dump:
        // - dump what’s actually used (so you see your 0xAA/0x5A fills)
        // - cap to keep output manageable
        size_t dump_n = used;
        if (dump_n > cap) dump_n = cap; // sanity clamp
        if (dump_n > ARENA_DUMP_MAX_BYTES_PER_BLOCK) dump_n = ARENA_DUMP_MAX_BYTES_PER_BLOCK;

        if (dump_n == 0) {
            printf("    (no used bytes)\n");
        } else {
            printf("    dump first %zu byte(s) of data:\n", dump_n);
            hexdump_lines(b->data, dump_n);
        }

        b = b->next;
        i++;
    }
}
