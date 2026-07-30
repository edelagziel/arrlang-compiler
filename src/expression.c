/**
 * @file expression.c
 * @brief Typed ArrLang expression construction and expression-level codegen.
 *
 * Responsibilities: allocate/free Expression objects, perform expression
 * semantic checks, propagate setup C statements, generate temporary arrays or
 * scalars through codegen, and create runtime checks for division and indexing.
 * This module does not emit declarations/assignments/print/control-flow
 * statements directly except through codegen helper calls.
 *
 * Main dependencies: codegen for temporary names and active statement emission,
 * compiler_context for semantic errors, symbol_table for identifier lookup, and
 * string_buffer for setup fragments. Typical flow: parser builds leaf
 * expressions, combines them through consuming builder functions, and finally
 * passes the resulting Expression to codegen or expression_emit_statement().
 */
#include "expression.h"

#include "codegen.h"
#include "compiler_context.h"
#include "symbol_table.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy == NULL) {
        fprintf(stderr, "Out of memory while copying text.\n");
        exit(1);
    }

    memcpy(copy, text, length);
    return copy;
}

/** Allocate formatted heap text used for generated C values and diagnostics. */
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

Expression *expression_create(ExprKind kind, int array_size, const char *value)
{
    Expression *expression = malloc(sizeof(*expression));

    if (expression == NULL) {
        fprintf(stderr, "Out of memory while creating expression.\n");
        exit(1);
    }

    expression->kind = kind;
    expression->array_size = array_size;
    expression->value = copy_string(value);
    string_buffer_init(&expression->setup);

    return expression;
}

/** Create a semantic-error placeholder so parsing can continue safely. */
Expression *expression_error(void)
{
    return expression_create(EXPR_ERROR, 0, "0");
}

void expression_free(Expression *expression)
{
    if (expression == NULL) {
        return;
    }

    free(expression->value);
    string_buffer_free(&expression->setup);
    free(expression);
}

Expression *expression_make_number(int value)
{
    char *text = format_string("%d", value);
    Expression *expression = expression_create(EXPR_SCALAR, 0, text);

    free(text);
    return expression;
}

Expression *expression_make_identifier(char *name)
{
    Symbol *symbol = symbol_table_lookup(name);
    Expression *expression;

    if (symbol == NULL) {
        compiler_context_report_semantic_error("use of undeclared variable '%s'", name);
        expression = expression_error();
    } else if (symbol->kind == SYMBOL_SCALAR) {
        expression = expression_create(EXPR_SCALAR, 0, name);
    } else {
        expression = expression_create(EXPR_ARRAY, symbol->array_size, name);
    }

    free(name);
    return expression;
}

Expression *expression_make_array_literal(Expression *elements)
{
    return elements;
}

Expression *expression_array_literal_start(int element)
{
    char *temp_name = codegen_new_array_temp(1);
    Expression *literal = expression_create(EXPR_ARRAY, 1, temp_name);

    string_buffer_appendf(&literal->setup, "    %s[0] = %d;\n", temp_name, element);
    free(temp_name);
    return literal;
}

/* Array literal growth preserves behavior by creating a new temporary array and
   copying the previous literal setup before appending the new element. */
Expression *expression_array_literal_append(Expression *literal, int element)
{
    char *old_name;
    char *new_name;
    char *index_name;
    Expression *expanded;

    if (literal->kind == EXPR_ERROR) {
        return literal;
    }

    old_name = copy_string(literal->value);
    new_name = codegen_new_array_temp(literal->array_size + 1);
    index_name = codegen_new_loop_index();
    expanded = expression_create(EXPR_ARRAY, literal->array_size + 1, new_name);
    string_buffer_append_buffer(&expanded->setup, &literal->setup);
    string_buffer_appendf(
        &expanded->setup,
        "    for (%s = 0; %s < %d; %s++) {\n"
        "        %s[%s] = %s[%s];\n"
        "    }\n"
        "    %s[%d] = %d;\n",
        index_name,
        index_name,
        literal->array_size,
        index_name,
        new_name,
        index_name,
        old_name,
        index_name,
        new_name,
        literal->array_size,
        element
    );

    expression_free(literal);
    free(old_name);
    free(new_name);
    free(index_name);
    return expanded;
}

Expression *expression_make_parenthesized(Expression *expression)
{
    char *value;
    Expression *result;

    if (expression->kind != EXPR_SCALAR) {
        return expression;
    }

    value = format_string("(%s)", expression->value);
    result = expression_create(EXPR_SCALAR, 0, value);
    string_buffer_append_buffer(&result->setup, &expression->setup);
    expression_free(expression);
    free(value);
    return result;
}

Expression *expression_make_unary_minus(Expression *expression)
{
    char *value;
    Expression *result;

    if (expression->kind == EXPR_ERROR) {
        return expression;
    }

    if (expression->kind != EXPR_SCALAR) {
        compiler_context_report_semantic_error("unary minus requires a scalar expression");
        expression_free(expression);
        return expression_error();
    }

    value = format_string("(-%s)", expression->value);
    result = expression_create(EXPR_SCALAR, 0, value);
    string_buffer_append_buffer(&result->setup, &expression->setup);
    expression_free(expression);
    free(value);
    return result;
}

Expression *expression_make_array_size(Expression *expression)
{
    char *value;
    Expression *result;

    if (expression->kind == EXPR_ERROR) {
        return expression;
    }

    if (expression->kind != EXPR_ARRAY) {
        compiler_context_report_semantic_error("size operator requires an array expression");
        expression_free(expression);
        return expression_error();
    }

    value = format_string("%d", expression->array_size);
    result = expression_create(EXPR_SCALAR, 0, value);
    string_buffer_append_buffer(&result->setup, &expression->setup);
    expression_free(expression);
    free(value);
    return result;
}

Expression *expression_make_index(Expression *array, Expression *index)
{
    char *temp_name;
    Expression *result;

    if (array->kind == EXPR_ERROR || index->kind == EXPR_ERROR) {
        expression_free(array);
        expression_free(index);
        return expression_error();
    }

    if (array->kind != EXPR_ARRAY || index->kind != EXPR_SCALAR) {
        compiler_context_report_semantic_error("index operator requires array on the left and scalar on the right");
        expression_free(array);
        expression_free(index);
        return expression_error();
    }

    /* Indexing is scalar-valued but needs setup code for bounds checking before
       the generated array access can be used. */
    temp_name = codegen_new_scalar_temp();
    result = expression_create(EXPR_SCALAR, 0, temp_name);
    string_buffer_append_buffer(&result->setup, &array->setup);
    string_buffer_append_buffer(&result->setup, &index->setup);
    string_buffer_appendf(
        &result->setup,
        "    if (%s < 0 || %s >= %d) {\n"
        "        fprintf(stderr, \"Runtime error: array index out of bounds\\n\");\n"
        "        return 1;\n"
        "    }\n"
        "    %s = %s[%s];\n",
        index->value,
        index->value,
        array->array_size,
        temp_name,
        array->value,
        index->value
    );

    expression_free(array);
    expression_free(index);
    free(temp_name);
    return result;
}

Expression *expression_make_binary(Expression *left, const char *operator_text, Expression *right)
{
    Expression *result;
    char *value;
    char *temp_name;
    char *index_name;

    if (left->kind == EXPR_ERROR || right->kind == EXPR_ERROR) {
        expression_free(left);
        expression_free(right);
        return expression_error();
    }

    /* Each branch consumes both operands and builds a result whose setup first
       evaluates operand setup, then any generated runtime checks or loops. */
    if (left->kind == EXPR_SCALAR && right->kind == EXPR_SCALAR) {
        if (strcmp(operator_text, "/") == 0) {
            temp_name = codegen_new_scalar_temp();
            result = expression_create(EXPR_SCALAR, 0, temp_name);
            string_buffer_append_buffer(&result->setup, &left->setup);
            string_buffer_append_buffer(&result->setup, &right->setup);
            string_buffer_appendf(
                &result->setup,
                "    if (%s == 0) {\n"
                "        fprintf(stderr, \"Runtime error: division by zero\\n\");\n"
                "        return 1;\n"
                "    }\n"
                "    %s = %s / %s;\n",
                right->value,
                temp_name,
                left->value,
                right->value
            );
            free(temp_name);
        } else {
            value = format_string("(%s %s %s)", left->value, operator_text, right->value);
            result = expression_create(EXPR_SCALAR, 0, value);
            string_buffer_append_buffer(&result->setup, &left->setup);
            string_buffer_append_buffer(&result->setup, &right->setup);
            free(value);
        }
    } else if (left->kind == EXPR_ARRAY && right->kind == EXPR_ARRAY) {
        if (left->array_size != right->array_size) {
            compiler_context_report_semantic_error("array operands must have the same size");
            expression_free(left);
            expression_free(right);
            return expression_error();
        }

        temp_name = codegen_new_array_temp(left->array_size);
        index_name = codegen_new_loop_index();
        result = expression_create(EXPR_ARRAY, left->array_size, temp_name);
        string_buffer_append_buffer(&result->setup, &left->setup);
        string_buffer_append_buffer(&result->setup, &right->setup);
        if (strcmp(operator_text, "/") == 0) {
            string_buffer_appendf(
                &result->setup,
                "    for (%s = 0; %s < %d; %s++) {\n"
                "        if (%s[%s] == 0) {\n"
                "            fprintf(stderr, \"Runtime error: division by zero\\n\");\n"
                "            return 1;\n"
                "        }\n"
                "        %s[%s] = %s[%s] / %s[%s];\n"
                "    }\n",
                index_name,
                index_name,
                left->array_size,
                index_name,
                right->value,
                index_name,
                temp_name,
                index_name,
                left->value,
                index_name,
                right->value,
                index_name
            );
        } else {
            string_buffer_appendf(
                &result->setup,
                "    for (%s = 0; %s < %d; %s++) {\n"
                "        %s[%s] = %s[%s] %s %s[%s];\n"
                "    }\n",
                index_name,
                index_name,
                left->array_size,
                index_name,
                temp_name,
                index_name,
                left->value,
                index_name,
                operator_text,
                right->value,
                index_name
            );
        }
        free(temp_name);
        free(index_name);
    } else if (left->kind == EXPR_ARRAY && right->kind == EXPR_SCALAR) {
        temp_name = codegen_new_array_temp(left->array_size);
        index_name = codegen_new_loop_index();
        result = expression_create(EXPR_ARRAY, left->array_size, temp_name);
        string_buffer_append_buffer(&result->setup, &left->setup);
        string_buffer_append_buffer(&result->setup, &right->setup);
        if (strcmp(operator_text, "/") == 0) {
            string_buffer_appendf(
                &result->setup,
                "    if (%s == 0) {\n"
                "        fprintf(stderr, \"Runtime error: division by zero\\n\");\n"
                "        return 1;\n"
                "    }\n",
                right->value
            );
        }
        string_buffer_appendf(
            &result->setup,
            "    for (%s = 0; %s < %d; %s++) {\n"
            "        %s[%s] = %s[%s] %s %s;\n"
            "    }\n",
            index_name,
            index_name,
            left->array_size,
            index_name,
            temp_name,
            index_name,
            left->value,
            index_name,
            operator_text,
            right->value
        );
        free(temp_name);
        free(index_name);
    } else {
        compiler_context_report_semantic_error("scalar-array arithmetic requires the array operand on the left");
        expression_free(left);
        expression_free(right);
        return expression_error();
    }

    expression_free(left);
    expression_free(right);
    return result;
}

Expression *expression_make_concat(Expression *left, Expression *right)
{
    Expression *result;
    char *temp_name;
    char *index_name;
    int result_size;

    if (left->kind == EXPR_ERROR || right->kind == EXPR_ERROR) {
        expression_free(left);
        expression_free(right);
        return expression_error();
    }

    if (left->kind != EXPR_ARRAY || right->kind != EXPR_ARRAY) {
        compiler_context_report_semantic_error("concatenation requires array operands");
        expression_free(left);
        expression_free(right);
        return expression_error();
    }

    result_size = left->array_size + right->array_size;
    temp_name = codegen_new_array_temp(result_size);
    index_name = codegen_new_loop_index();
    result = expression_create(EXPR_ARRAY, result_size, temp_name);
    string_buffer_append_buffer(&result->setup, &left->setup);
    string_buffer_append_buffer(&result->setup, &right->setup);
    string_buffer_appendf(
        &result->setup,
        "    for (%s = 0; %s < %d; %s++) {\n"
        "        %s[%s] = %s[%s];\n"
        "    }\n"
        "    for (%s = 0; %s < %d; %s++) {\n"
        "        %s[%s + %d] = %s[%s];\n"
        "    }\n",
        index_name,
        index_name,
        left->array_size,
        index_name,
        temp_name,
        index_name,
        left->value,
        index_name,
        index_name,
        index_name,
        right->array_size,
        index_name,
        temp_name,
        index_name,
        left->array_size,
        right->value,
        index_name
    );

    expression_free(left);
    expression_free(right);
    free(temp_name);
    free(index_name);
    return result;
}

/** Expression statements execute setup effects and explicitly discard the final
    generated value so generated C stays warning-clean when possible. */
void expression_emit_statement(Expression *expression)
{
    if (expression->kind != EXPR_ERROR) {
        codegen_emit_setup(&expression->setup);
        if (expression->kind == EXPR_SCALAR) {
            codegen_emit_active("    (void)(%s);\n", expression->value);
        } else {
            codegen_emit_active("    (void)%s;\n", expression->value);
        }
    }

    expression_free(expression);
}
