/**
 * @file compiler_context.c
 * @brief Shared output path and error counters for one compilation.
 *
 * Responsibilities: remember the requested output path, count semantic and
 * lexical errors, and format semantic diagnostics with source line numbers.
 * This module does not own input files, generated code buffers, or symbol-table
 * memory.
 *
 * Main dependencies: yylineno from Flex/Bison and stdio for diagnostics.
 * Typical flow: initialize once in main, report errors during parsing, query
 * final count before writing output.
 */
#include "compiler_context.h"

#include <stdarg.h>
#include <stdio.h>

extern int yylineno;

static const char *current_output_path = NULL;
static int semantic_errors = 0;
static int lexical_errors = 0;

void compiler_context_init(const char *output_path)
{
    current_output_path = output_path;
    semantic_errors = 0;
    lexical_errors = 0;
}

const char *compiler_context_output_path(void)
{
    return current_output_path;
}

void compiler_context_report_semantic_error(const char *format, ...)
{
    va_list args;

    fprintf(stderr, "Semantic error on line %d: ", yylineno);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
    semantic_errors++;
}

void compiler_context_add_lexical_error(void)
{
    lexical_errors++;
}

int compiler_context_error_count(void)
{
    return semantic_errors + lexical_errors;
}
