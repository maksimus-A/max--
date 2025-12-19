#pragma once
#include "ast/lexer/lexer.h"

void print_all_tokens(TokenBuffer* tokens, const char *buffer);
void pretty_print_tokens(TokenBuffer* tokens, const char *buffer);