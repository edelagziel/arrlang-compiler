/**
 * @file expression.h
 * @brief Typed expression representation and builders for ArrLang.
 *
 * This module creates and combines semantic Expression objects while generating
 * any setup C code needed to evaluate them. It performs expression-level
 * semantic checks such as scalar/array operand compatibility, array-size
 * matching, indexing rules, division-by-zero runtime checks, and temporary array
 * creation through codegen. It does not emit declarations, assignments, print
 * statements, if/loop statements, or final C files.
 *
 * Main dependencies: string_buffer for setup code, symbol_table for identifier
 * lookup, compiler_context for diagnostics, and codegen for temporary names.
 *
 * Typical usage:
 * @code
 * Expression *left = expression_make_number(5);
 * Expression *right = expression_make_number(3);
 * Expression *sum = expression_make_binary(left, "+", right);
 * // left and right are consumed by expression_make_binary().
 * expression_free(sum);
 * @endcode
 */
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "string_buffer.h"

/**
 * @brief Result type of an ArrLang expression.
 */
typedef enum {
    EXPR_ERROR,  /**< Invalid expression after a semantic error. */
    EXPR_SCALAR, /**< Expression producing one integer value. */
    EXPR_ARRAY   /**< Expression producing an integer array. */
} ExprKind;

/**
 * @brief Semantic and code-generation representation of one expression.
 *
 * @var Expression::kind
 * Result kind. EXPR_ERROR is used as a recoverable placeholder after semantic
 * errors so parsing can continue.
 *
 * @var Expression::array_size
 * Number of elements when kind is EXPR_ARRAY. Zero for scalar/error
 * expressions.
 *
 * @var Expression::value
 * Heap-owned C value text copied by expression constructors. For scalars this
 * is a C expression or temporary scalar name; for arrays this is an array
 * variable or temporary array name. Freed by expression_free().
 *
 * @var Expression::setup
 * Owned StringBuffer containing C statements that must run before using
 * @ref value. Freed by expression_free().
 *
 * Ownership rules: constructors copy input strings. Functions that combine
 * expressions consume their Expression operands unless the specific function
 * returns the same pointer unchanged for parenthesized array expressions.
 */
typedef struct Expression {
    ExprKind kind;
    int array_size;
    char *value;
    StringBuffer setup;
} Expression;

/**
 * @brief Create an expression with copied value text.
 *
 * @param kind Expression result kind.
 * @param array_size Array size for EXPR_ARRAY, zero otherwise.
 * @param value Borrowed pointer; must not be NULL. Copied internally.
 *
 * @return Caller owns returned Expression* and must free it with
 *         expression_free().
 */
Expression *expression_create(ExprKind kind, int array_size, const char *value);

/**
 * @brief Create a recoverable error expression.
 *
 * @return Caller owns returned Expression*. Its value is harmless placeholder
 *         text and should not be emitted after errors.
 */
Expression *expression_error(void);

/**
 * @brief Free an expression and all owned memory.
 *
 * @param expression Ownership transferred; may be NULL. Frees value, setup
 *        buffer, and the Expression object.
 */
void expression_free(Expression *expression);

/**
 * @brief Build a scalar integer literal expression.
 *
 * @param value Integer literal value.
 * @return Caller owns returned scalar Expression*.
 */
Expression *expression_make_number(int value);

/**
 * @brief Build an expression for a declared identifier.
 *
 * @param name Ownership transferred; must not be NULL. Freed by the function.
 *
 * Performs symbol lookup. Undeclared variables produce a semantic error and an
 * EXPR_ERROR expression.
 *
 * @return Caller owns returned Expression*.
 */
Expression *expression_make_identifier(char *name);

/**
 * @brief Finalize an array literal expression.
 *
 * @param elements Ownership transferred; must not be NULL.
 * @return The same owned Expression* is returned to the caller.
 */
Expression *expression_make_array_literal(Expression *elements);

/**
 * @brief Start an array literal with one integer element.
 *
 * @param element First literal element.
 * @return Caller owns returned array Expression*.
 */
Expression *expression_array_literal_start(int element);

/**
 * @brief Append one integer element to an array literal expression.
 *
 * @param literal Ownership transferred; must not be NULL.
 * @param element Element to append.
 *
 * Creates a new temporary array and consumes the previous literal expression.
 *
 * @return Caller owns returned array Expression*.
 */
Expression *expression_array_literal_append(Expression *literal, int element);

/**
 * @brief Apply parentheses to a scalar expression.
 *
 * @param expression Ownership transferred; must not be NULL.
 * @return Caller owns returned Expression*. Array expressions may be returned
 *         unchanged because parentheses do not affect their generated value.
 */
Expression *expression_make_parenthesized(Expression *expression);

/**
 * @brief Apply unary minus to a scalar expression.
 *
 * @param expression Ownership transferred; must not be NULL.
 * @return Caller owns returned scalar or error Expression*.
 */
Expression *expression_make_unary_minus(Expression *expression);

/**
 * @brief Build the scalar size expression for an array.
 *
 * @param expression Ownership transferred; must not be NULL.
 * @return Caller owns returned scalar or error Expression*.
 */
Expression *expression_make_array_size(Expression *expression);

/**
 * @brief Build a scalar array-indexing expression with runtime bounds check.
 *
 * @param array Ownership transferred; must not be NULL and should produce an
 *        array.
 * @param index Ownership transferred; must not be NULL and should produce a
 *        scalar.
 *
 * Emits setup code that checks `index < 0 || index >= size` and returns 1 from
 * generated C main on failure.
 *
 * @return Caller owns returned scalar or error Expression*.
 */
Expression *expression_make_index(Expression *array, Expression *index);

/**
 * @brief Build scalar/array arithmetic for +, -, *, or /.
 *
 * @param left Ownership transferred; must not be NULL.
 * @param operator_text Borrowed pointer; must not be NULL and should be one of
 *        "+", "-", "*", "/".
 * @param right Ownership transferred; must not be NULL.
 *
 * Supports scalar-scalar, array-array of equal sizes, and array-scalar. Division
 * emits runtime zero checks. Scalar-array is rejected to match current language
 * behavior.
 *
 * @return Caller owns returned Expression*.
 */
Expression *expression_make_binary(Expression *left, const char *operator_text, Expression *right);

/**
 * @brief Concatenate two array expressions.
 *
 * @param left Ownership transferred; must not be NULL and should be array.
 * @param right Ownership transferred; must not be NULL and should be array.
 * @return Caller owns returned array or error Expression*.
 */
Expression *expression_make_concat(Expression *left, Expression *right);

/**
 * @brief Emit setup code for a standalone expression statement and discard it.
 *
 * @param expression Ownership transferred; may be an error expression.
 *
 * Emits `(void)` use of the final value to avoid unused-temporary warnings where
 * appropriate, then frees the expression. No returned ownership.
 */
void expression_emit_statement(Expression *expression);

#endif
