#pragma once

#include "ast/parser/ast.h"
#include "codegen/ir-gen/ir_types.h"
#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_ERR_MSG_SIZE 200

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

// two main funcs (gonna try to only use create_and_add...)
void add_diag(Diagnostics* diags, Severity sev, SrcSpan span, char* err_msg, size_t line, size_t col);
void create_and_add_diag_fmt(Diagnostics* diags, Severity sev, SrcSpan span, const char* fmt, Source* source_file);
void diags_init(Diagnostics* diags, Arena* arena, size_t capacity);

void add_diag_mir_inst(Diagnostics* diags, Severity sev, size_t inst_num, IRInstructType inst_type, char* err_msg);
void add_diag_mir(Diagnostics* diags,
                  Severity sev,
                  const char* fmt,
                  ...);

char* alloc_error(Diagnostics* diags);

bool print_errors(Diagnostics* diags, const char* pass);
