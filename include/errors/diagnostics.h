
#include "ast/parser/ast.h"
#include <stddef.h>

#define DEFAULT_ERR_MSG_SIZE 100

typedef enum Severity {
    WARN,
    ERROR
} Severity;

typedef struct Diagnostic {
    Severity severity;
    SrcSpan span;
    const char* err_msg;
} Diagnostic;

typedef struct Diagnostics {
    size_t count, capacity;
    Diagnostic** items;
    Arena* arena;
} Diagnostics;

Diagnostic* create_diag(Arena* arena, Severity sev, SrcSpan span, const char* err_msg);

void push_error(Diagnostics* diags, Diagnostic* diag);
void add_diag(Diagnostics* diags, Severity sev, SrcSpan span, char* err_msg, size_t line, size_t col);
void diags_init(Diagnostics* diags, Arena* arena, size_t capacity);
