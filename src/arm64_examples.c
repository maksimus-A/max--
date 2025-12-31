#include <stdint.h>

/*
// --------------------
// 1) Recursive Fibonacci
// --------------------
__attribute__((noinline))
int64_t fib_rec(int64_t n) {
    if (n <= 1) return n;
    return fib_rec(n - 1) + fib_rec(n - 2);
}

// --------------------
// 2) Circle computations (floating-point)
//    Return two doubles to see multi-value return in FP regs.
// --------------------
typedef struct CircleMetrics {
    double area;
    double circumference;
} CircleMetrics;

__attribute__((noinline))
CircleMetrics circle_metrics(double r) {
    const double pi = 3.14159265358979323846;
    CircleMetrics out;
    out.area = pi * r * r;
    out.circumference = 2.0 * pi * r;
    return out;
}

// --------------------
// 3) Switch + loop (teaches branch patterns / jump tables)
//    Often becomes a jump table at -O2.
// --------------------
__attribute__((noinline))
int32_t mix_switch_and_loop(int32_t x, int32_t n) {
    int32_t acc = 0;

    // Counted loop with simple arithmetic
    for (int32_t i = 0; i < n; i++) {
        acc += (x ^ i) + (x & 7);
        x = (x << 1) | (x >> 31); // rotate-ish (shows shifts/or)
    }

    // Switch (good for seeing compare/branch vs jump table)
    switch (acc & 7) {
        case 0: return acc + 10;
        case 1: return acc - 20;
        case 2: return acc ^ 0x1234;
        case 3: return acc * 3;
        case 4: return acc / 5;
        case 5: return acc + (n << 2);
        case 6: return acc - (n >> 1);
        default: return acc;
    }
}*/

typedef struct Test {
    int x;
    char y;
}Test;

__attribute__((noinline))
Test test() {
    Test t = (Test){
        .x = 1,
        .y = 'a'
    };
    return t;
}

__attribute__((noinline))
void test_user() {
    Test t = test();
    t.x++;
}

/*
__attribute__((noinline))
int mx_int_decl() {
    int x = 1;
    int y = 2;
    x = y;
    y = 3;
    return x;
}*/
