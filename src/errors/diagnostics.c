#include "errors/diagnostics.h"
#include <stdalign.h>
#include <stdbool.h>
#include <string.h>

void diags_init(Diagnostics* diags, Arena* arena, size_t capacity) {
    diags->arena = arena;
    diags->count = 0;
    diags->capacity = capacity;
    diags->items = (Diagnostic**)arena_alloc(arena, sizeof(Diagnostic*) * diags->capacity, alignof(Diagnostic*));
}

void ensure_diag_item_capacity(Diagnostics* diags) {
    if (diags->capacity == diags->count) {
        size_t new_capacity = diags->capacity * 2;
        Diagnostic** new_items = (Diagnostic**)arena_alloc(diags->arena, new_capacity * sizeof(Diagnostic*), alignof(Diagnostic*));
        if (!new_items) {
            // TODO: Error handle better.
            fprintf(stderr, "ERROR: Could not allocate new items list for 'diagonstics' from arena.");
            return;
        }
        if (diags->count > 0) {
            memcpy(new_items, diags->items, diags->count * sizeof(Diagnostic*));
        }
        diags->capacity = new_capacity;
        diags->items = new_items;
        return;
    }
}

void push_error(Diagnostics* diags, Diagnostic* diag) {
    ensure_diag_item_capacity(diags);

    size_t count = diags->count;
    // Assumes diag was already filled
    diags->items[count] = diag;
    diags->count++;
}

// Creates diagnostic for error reporting inside arena.
// Assumes error was fully crafted.
Diagnostic* create_diag(Arena* arena, Severity sev, SrcSpan span, const char* err_msg) {
    Diagnostic* diag = arena_alloc(arena, sizeof(Diagnostic), alignof(Diagnostic));
    diag->err_msg = err_msg;
    diag->severity = sev;
    diag->span = span;

    return diag;
}

char* alloc_error(Diagnostics* diags) {
    return (char*)arena_alloc(diags->arena,
                            DEFAULT_ERR_MSG_SIZE,
                            alignof(char));
}

// Adds error message with line/col to diagnostics item list.
// Takes existing error, adds line/col info to beginning.
void add_diag(Diagnostics* diags, Severity sev, SrcSpan span, char* err_msg, size_t line, size_t col) {
    char* new_err_msg = alloc_error(diags);
    if (!new_err_msg) return;
    snprintf(new_err_msg, DEFAULT_ERR_MSG_SIZE, "Error at (%zu:%zu): %s", line, col, err_msg);

    Diagnostic* diag = create_diag(diags->arena, sev, span, new_err_msg);
    if (!diag) return;

    push_error(diags, diag);
}

bool print_errors(Diagnostics* diags) {
    if (diags->count == 0) return false;
    for (int i = 0; i < diags->count; i++) {
        char* severity = diags->items[i]->severity == ERROR ? "ERROR:" : "WARN";
        fprintf(stderr, "%s %s\n", severity, diags->items[i]->err_msg);
    }
}

