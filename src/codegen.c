/**
 * @file codegen.c
 * @brief Generated C buffers, temporaries, and statement emitters.
 *
 * Responsibilities: own declaration and statement buffers, track the active
 * statement destination, create unique temporary names, consume expressions and
 * statement blocks, emit C for statements, and write the final C source file.
 * This module does not parse ArrLang or build expression internals.
 *
 * Main dependencies: compiler_context for output path/errors, expression for
 * typed values, statement_block for nested buffers, symbol_table for
 * declaration/assignment validation, and string_buffer for text accumulation.
 * Typical flow: codegen_init(), parser delegates statement emission during
 * yyparse(), codegen_write_output(), codegen_free().
 */
#include "codegen.h"

#include "compiler_context.h"
#include "statement_block.h"
#include "symbol_table.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static StringBuffer declarations;
static StringBuffer statements;
static StringBuffer *active_statements = NULL;
static int next_array_temp = 1;
static int next_scalar_temp = 1;
static int next_loop_index = 1;

static char *format_string(const char *format, ...)
{
    va_list args;
    va_list args_copy;
    int required;
    char *result;

    va_start(args, format);
    va_copy(args_copy, args);
    required = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (required < 0) {
        fprintf(stderr, "Internal error while formatting text.\n");
        exit(1);
    }

    result = malloc((size_t)required + 1);
    if (result == NULL) {
        fprintf(stderr, "Out of memory while formatting text.\n");
        exit(1);
    }

    vsnprintf(result, (size_t)required + 1, format, args);
    va_end(args);

    return result;
}

/** Initialize module-global generation state for one compilation. */
void codegen_init(void)
{
    string_buffer_init(&declarations);
    string_buffer_init(&statements);
    active_statements = &statements;
    next_array_temp = 1;
    next_scalar_temp = 1;
    next_loop_index = 1;
}

void codegen_free(void)
{
    string_buffer_free(&declarations);
    string_buffer_free(&statements);
    active_statements = NULL;
}

char *codegen_new_array_temp(int size)
{
    char *name = format_string("__arr_tmp_%d", next_array_temp++);

    string_buffer_appendf(&declarations, "    int %s[%d];\n", name, size);
    return name;
}

/** Temporary names are declared immediately so later setup code can use them. */
char *codegen_new_scalar_temp(void)
{
    char *name = format_string("__scl_tmp_%d", next_scalar_temp++);

    string_buffer_appendf(&declarations, "    int %s;\n", name);
    return name;
}

char *codegen_new_loop_index(void)
{
    char *name = format_string("__i_%d", next_loop_index++);

    string_buffer_appendf(&declarations, "    int %s;\n", name);
    return name;
}

void codegen_emit_setup(const StringBuffer *setup)
{
    codegen_append_active_buffer(setup);
}

void codegen_emit_active(const char *format, ...)
{
    va_list args;
    va_list args_copy;
    int required;
    char *text;

    va_start(args, format);
    va_copy(args_copy, args);
    required = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (required < 0) {
        fprintf(stderr, "Internal error while formatting generated code.\n");
        exit(1);
    }

    text = malloc((size_t)required + 1);
    if (text == NULL) {
        fprintf(stderr, "Out of memory while formatting generated code.\n");
        exit(1);
    }

    vsnprintf(text, (size_t)required + 1, format, args);
    va_end(args);
    string_buffer_appendf(active_statements, "%s", text);
    free(text);
}

void codegen_append_active_buffer(const StringBuffer *source)
{
    string_buffer_append_buffer(active_statements, source);
}

StringBuffer *codegen_set_active_buffer(StringBuffer *buffer)
{
    StringBuffer *previous = active_statements;

    active_statements = buffer;
    return previous;
}

/* Declaration emitters consume lexer-owned identifier strings and keep the
   symbol table and generated C declaration buffer in sync. */
void codegen_declare_scalar(char *name)
{
    int result = symbol_table_insert(name, SYMBOL_SCALAR, 0);

    if (result == 0) {
        compiler_context_report_semantic_error("duplicate declaration of '%s'", name);
    } else if (result < 0) {
        fprintf(stderr, "Out of memory while declaring '%s'.\n", name);
        exit(1);
    } else {
        string_buffer_appendf(&declarations, "    int %s;\n", name);
    }

    free(name);
}

void codegen_declare_array(char *name, int size)
{
    int result;

    if (size <= 0) {
        compiler_context_report_semantic_error("array '%s' must have a positive size", name);
        free(name);
        return;
    }

    result = symbol_table_insert(name, SYMBOL_ARRAY, size);
    if (result == 0) {
        compiler_context_report_semantic_error("duplicate declaration of '%s'", name);
    } else if (result < 0) {
        fprintf(stderr, "Out of memory while declaring '%s'.\n", name);
        exit(1);
    } else {
        string_buffer_appendf(&declarations, "    int %s[%d];\n", name, size);
    }

    free(name);
}

void codegen_assign_expression(char *name, Expression *expression)
{
    Symbol *symbol = symbol_table_lookup(name);
    char *index_name;

    /* Assignment consumes both the target name and expression. Setup code is
       emitted before the final scalar assignment or array-copy loop. */
    if (symbol == NULL) {
        compiler_context_report_semantic_error("assignment to undeclared variable '%s'", name);
    } else if (expression->kind != EXPR_ERROR) {
        if (symbol->kind == SYMBOL_SCALAR && expression->kind != EXPR_SCALAR) {
            compiler_context_report_semantic_error("cannot assign an array expression to scalar '%s'", name);
        } else if (symbol->kind == SYMBOL_ARRAY && expression->kind != EXPR_ARRAY) {
            compiler_context_report_semantic_error("cannot assign a scalar expression to array '%s'", name);
        } else if (symbol->kind == SYMBOL_ARRAY && expression->array_size != symbol->array_size) {
            compiler_context_report_semantic_error(
                "array expression for '%s' has size %d but declared size is %d",
                name,
                expression->array_size,
                symbol->array_size
            );
        } else if (symbol->kind == SYMBOL_SCALAR) {
            codegen_emit_setup(&expression->setup);
            codegen_emit_active("    %s = %s;\n", name, expression->value);
        } else {
            index_name = codegen_new_loop_index();
            codegen_emit_setup(&expression->setup);
            codegen_emit_active(
                "    for (%s = 0; %s < %d; %s++) {\n"
                "        %s[%s] = %s[%s];\n"
                "    }\n",
                index_name,
                index_name,
                symbol->array_size,
                index_name,
                name,
                index_name,
                expression->value,
                index_name
            );
            free(index_name);
        }
    }

    free(name);
    expression_free(expression);
}

void codegen_reverse_array(char *name)
{
    Symbol *symbol = symbol_table_lookup(name);
    char *index_name;
    char *temp_name;

    if (symbol == NULL) {
        compiler_context_report_semantic_error("reverse of undeclared variable '%s'", name);
    } else if (symbol->kind != SYMBOL_ARRAY) {
        compiler_context_report_semantic_error("reverse operator requires array operand '%s'", name);
    } else {
        index_name = codegen_new_loop_index();
        temp_name = codegen_new_scalar_temp();
        codegen_emit_active(
            "    for (%s = 0; %s < %d / 2; %s++) {\n"
            "        %s = %s[%s];\n"
            "        %s[%s] = %s[%d - 1 - %s];\n"
            "        %s[%d - 1 - %s] = %s;\n"
            "    }\n",
            index_name,
            index_name,
            symbol->array_size,
            index_name,
            temp_name,
            name,
            index_name,
            name,
            index_name,
            name,
            symbol->array_size,
            index_name,
            name,
            symbol->array_size,
            index_name,
            temp_name
        );
        free(index_name);
        free(temp_name);
    }

    free(name);
}

void codegen_sort_array(char *name)
{
    Symbol *symbol = symbol_table_lookup(name);
    char *i_name;
    char *j_name;
    char *temp_name;

    if (symbol == NULL) {
        compiler_context_report_semantic_error("sort of undeclared variable '%s'", name);
    } else if (symbol->kind != SYMBOL_ARRAY) {
        compiler_context_report_semantic_error("sort operator requires array operand '%s'", name);
    } else {
        i_name = codegen_new_loop_index();
        j_name = codegen_new_loop_index();
        temp_name = codegen_new_scalar_temp();
        codegen_emit_active(
            "    for (%s = 0; %s < %d - 1; %s++) {\n"
            "        for (%s = 0; %s < %d - 1 - %s; %s++) {\n"
            "            if (%s[%s] > %s[%s + 1]) {\n"
            "                %s = %s[%s];\n"
            "                %s[%s] = %s[%s + 1];\n"
            "                %s[%s + 1] = %s;\n"
            "            }\n"
            "        }\n"
            "    }\n",
            i_name,
            i_name,
            symbol->array_size,
            i_name,
            j_name,
            j_name,
            symbol->array_size,
            i_name,
            j_name,
            name,
            j_name,
            name,
            j_name,
            temp_name,
            name,
            j_name,
            name,
            j_name,
            name,
            j_name,
            name,
            j_name,
            temp_name
        );
        free(i_name);
        free(j_name);
        free(temp_name);
    }

    free(name);
}

void codegen_begin_print(char *label)
{
    if (strcmp(label, "\"\"") != 0) {
        codegen_emit_active("    printf(\"%%s: \", %s);\n", label);
    }
    free(label);
}

void codegen_print_separator(void)
{
    codegen_emit_active("    printf(\", \");\n");
}

void codegen_print_expression(Expression *expression)
{
    char *index_name;

    if (expression->kind == EXPR_ERROR) {
        expression_free(expression);
        return;
    }

    codegen_emit_setup(&expression->setup);

    /* Arrays are printed with a generated loop so runtime values, not compile
       time literals, are displayed. */
    if (expression->kind == EXPR_SCALAR) {
        codegen_emit_active("    printf(\"%%d\", %s);\n", expression->value);
    } else {
        index_name = codegen_new_loop_index();
        codegen_emit_active(
            "    printf(\"[\");\n"
            "    for (%s = 0; %s < %d; %s++) {\n"
            "        if (%s > 0) {\n"
            "            printf(\", \");\n"
            "        }\n"
            "        printf(\"%%d\", %s[%s]);\n"
            "    }\n"
            "    printf(\"]\");\n",
            index_name,
            index_name,
            expression->array_size,
            index_name,
            index_name,
            expression->value,
            index_name
        );
        free(index_name);
    }

    expression_free(expression);
}

void codegen_end_print(void)
{
    codegen_emit_active("    printf(\"\\n\");\n");
}

void codegen_emit_if(Expression *condition, StatementBlock *then_block, StatementBlock *else_block)
{
    /* Blocks are consumed here regardless of success because the parser hands
       ownership to codegen once the complete if statement is recognized. */
    if (condition->kind == EXPR_ERROR) {
        expression_free(condition);
        statement_block_free(then_block);
        statement_block_free(else_block);
        return;
    }

    if (condition->kind != EXPR_SCALAR) {
        compiler_context_report_semantic_error("if condition must be a scalar expression");
    } else {
        codegen_emit_setup(&condition->setup);
        codegen_emit_active("    if (%s) {\n", condition->value);
        codegen_append_active_buffer(&then_block->buffer);
        codegen_emit_active("    }");

        if (else_block != NULL) {
            codegen_emit_active(" else {\n");
            codegen_append_active_buffer(&else_block->buffer);
            codegen_emit_active("    }");
        }

        codegen_emit_active("\n");
    }

    expression_free(condition);
    statement_block_free(then_block);
    statement_block_free(else_block);
}

void codegen_emit_loop(Expression *count, StatementBlock *body)
{
    char *index_name;
    char *count_name;

    /* The count expression is evaluated once into a scalar temp to avoid
       re-running setup code on every C loop condition check. */
    if (count->kind == EXPR_ERROR) {
        expression_free(count);
        statement_block_free(body);
        return;
    }

    if (count->kind != EXPR_SCALAR) {
        compiler_context_report_semantic_error("loop count must be a scalar expression");
    } else {
        index_name = codegen_new_loop_index();
        count_name = codegen_new_scalar_temp();
        codegen_emit_setup(&count->setup);
        codegen_emit_active("    %s = %s;\n", count_name, count->value);
        codegen_emit_active(
            "    for (%s = 0; %s < %s; %s++) {\n",
            index_name,
            index_name,
            count_name,
            index_name
        );
        codegen_append_active_buffer(&body->buffer);
        codegen_emit_active("    }\n");
        free(index_name);
        free(count_name);
    }

    expression_free(count);
    statement_block_free(body);
}

int codegen_write_output(void)
{
    const char *output_path = compiler_context_output_path();
    FILE *output = fopen(output_path, "w");

    if (output == NULL) {
        perror(output_path);
        return 1;
    }

    /* The final output is written only after main.c has checked that parsing
       finished with no lexical or semantic errors. */
    fprintf(output, "#include <stdio.h>\n\n");
    fprintf(output, "int main(void)\n");
    fprintf(output, "{\n");

    if (declarations.data != NULL) {
        fputs(declarations.data, output);
    }

    if (declarations.length > 0 && statements.length > 0) {
        fputc('\n', output);
    }

    if (statements.data != NULL) {
        fputs(statements.data, output);
    }

    fprintf(output, "\n    return 0;\n");
    fprintf(output, "}\n");

    fclose(output);
    return 0;
}
