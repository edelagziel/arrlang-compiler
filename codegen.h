/**
 * @file codegen.h
 * @brief C code generation state and statement emitters for ArrLang.
 *
 * This module owns the generated declaration buffer, top-level statement
 * buffer, active statement destination, and counters for unique temporary names.
 * It emits C for declarations, assignments, reverse/sort, print, if/else, loop,
 * expression setup, and final output writing. It does not scan tokens, parse
 * grammar, store symbol-table entries directly, or construct expression setup
 * logic beyond consuming completed Expression objects.
 *
 * Main dependencies: expression for typed values, string_buffer for generated
 * text, symbol_table for declaration lookup, compiler_context for diagnostics
 * and output path, and statement_block for nested captured bodies.
 *
 * Typical lifecycle:
 * @code
 * compiler_context_init(output_path);
 * codegen_init();
 * symbol_table_init();
 * yyparse();
 * codegen_write_output();
 * codegen_free();
 * symbol_table_free();
 * @endcode
 */
#ifndef CODEGEN_H
#define CODEGEN_H

#include "expression.h"
#include "string_buffer.h"

typedef struct StatementBlock StatementBlock;

/**
 * @brief Initialize all code-generation buffers and temporary counters.
 *
 * Must be called before any other codegen function. No returned ownership.
 */
void codegen_init(void);

/**
 * @brief Free all generated-code buffers owned by codegen.
 *
 * Invalidates any active buffer pointers owned internally. No returned
 * ownership.
 */
void codegen_free(void);

/**
 * @brief Declare and return a unique temporary array name.
 *
 * @param size Positive number of integer elements.
 * @return Caller owns returned heap string and must free it. The declaration is
 *         appended to the generated declaration buffer.
 */
char *codegen_new_array_temp(int size);

/**
 * @brief Declare and return a unique temporary scalar name.
 *
 * @return Caller owns returned heap string and must free it.
 */
char *codegen_new_scalar_temp(void);

/**
 * @brief Declare and return a unique loop-index variable name.
 *
 * @return Caller owns returned heap string and must free it.
 */
char *codegen_new_loop_index(void);

/**
 * @brief Emit expression setup statements to the current active destination.
 *
 * @param setup Borrowed pointer; must not be NULL. Contents are copied.
 * No returned ownership.
 */
void codegen_emit_setup(const StringBuffer *setup);

/**
 * @brief Append formatted C statements to the current active destination.
 *
 * @param format Borrowed printf-style format string; must not be NULL.
 * @param ... Format arguments matching @p format.
 * No returned ownership.
 */
void codegen_emit_active(const char *format, ...);

/**
 * @brief Append an existing buffer to the current active destination.
 *
 * @param source Borrowed pointer; must not be NULL. Contents are copied.
 * No returned ownership.
 */
void codegen_append_active_buffer(const StringBuffer *source);

/**
 * @brief Replace the active statement destination.
 *
 * @param buffer Borrowed pointer; may be NULL only during cleanup-like use, but
 *        normal parsing passes an initialized buffer.
 * @return Borrowed pointer to the previous active buffer. The caller must not
 *         free it unless it owns the underlying buffer object.
 */
StringBuffer *codegen_set_active_buffer(StringBuffer *buffer);

/**
 * @brief Declare a scalar variable in the symbol table and generated C.
 *
 * @param name Ownership transferred; must not be NULL. Freed by this function.
 * Reports duplicate declarations as semantic errors.
 */
void codegen_declare_scalar(char *name);

/**
 * @brief Declare an array variable in the symbol table and generated C.
 *
 * @param name Ownership transferred; must not be NULL. Freed by this function.
 * @param size Declared array size. Non-positive sizes are semantic errors.
 */
void codegen_declare_array(char *name, int size);

/**
 * @brief Emit an assignment after validating target and expression types.
 *
 * @param name Ownership transferred; must not be NULL. Freed by this function.
 * @param expression Ownership transferred; must not be NULL. Freed by this
 *        function.
 *
 * Emits expression setup before assignment. Array assignments copy elements in a
 * generated loop.
 */
void codegen_assign_expression(char *name, Expression *expression);

/**
 * @brief Emit in-place reverse code for a declared array.
 *
 * @param name Ownership transferred; must not be NULL. Freed by this function.
 * Reports undeclared or non-array operands as semantic errors.
 */
void codegen_reverse_array(char *name);

/**
 * @brief Emit in-place bubble sort code for a declared array.
 *
 * @param name Ownership transferred; must not be NULL. Freed by this function.
 * Reports undeclared or non-array operands as semantic errors.
 */
void codegen_sort_array(char *name);

/**
 * @brief Start a print statement by emitting its optional label.
 *
 * @param label Ownership transferred; must not be NULL. It is a quoted C string
 *        literal token and is freed by this function. Empty prompt `""` emits
 *        no label or colon.
 */
void codegen_begin_print(char *label);

/**
 * @brief Emit separator text between print arguments.
 *
 * No parameters. No returned ownership.
 */
void codegen_print_separator(void);

/**
 * @brief Emit C printing code for one scalar or array expression.
 *
 * @param expression Ownership transferred; must not be NULL. Freed by this
 *        function. Emits setup code before printing.
 */
void codegen_print_expression(Expression *expression);

/**
 * @brief Finish a print statement by emitting a newline.
 *
 * No parameters. No returned ownership.
 */
void codegen_end_print(void);

/**
 * @brief Emit an if or if/else statement.
 *
 * @param condition Ownership transferred; must not be NULL and should be scalar.
 * @param then_block Ownership transferred; must not be NULL.
 * @param else_block Ownership transferred; may be NULL.
 *
 * The condition setup is emitted before the C if. Consumes and frees all passed
 * objects.
 */
void codegen_emit_if(Expression *condition, StatementBlock *then_block, StatementBlock *else_block);

/**
 * @brief Emit a counted loop statement.
 *
 * @param count Ownership transferred; must not be NULL and should be scalar.
 * @param body Ownership transferred; must not be NULL.
 *
 * The loop count is evaluated once into a temporary scalar before the C for
 * loop. Consumes and frees both objects.
 */
void codegen_emit_loop(Expression *count, StatementBlock *body);

/**
 * @brief Write the final generated C file.
 *
 * @return 0 on success, 1 if the output file cannot be opened. No returned
 *         ownership.
 */
int codegen_write_output(void);

#endif
