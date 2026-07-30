/**
 * @file compiler_context.h
 * @brief Compiler-wide output path and error accounting.
 *
 * This module stores state that must be visible across the lexer, parser,
 * semantic checks, and final output writer. It is responsible for the requested
 * output C path and for counting lexical and semantic errors. It does not own
 * symbol declarations, generated code buffers, expressions, or input files.
 *
 * Main dependencies: standard I/O for diagnostic reporting.
 *
 * Typical lifecycle:
 * @code
 * compiler_context_init(output_path);
 * codegen_init();
 * symbol_table_init();
 * yyparse();
 * if (compiler_context_error_count() == 0) {
 *     codegen_write_output();
 * }
 * codegen_free();
 * symbol_table_free();
 * @endcode
 */
#ifndef COMPILER_CONTEXT_H
#define COMPILER_CONTEXT_H

/**
 * @brief Reset compiler-wide state for one compilation.
 *
 * @param output_path Borrowed pointer; must not be NULL and must remain valid
 *        until codegen_write_output() finishes. The string is not copied.
 *
 * Clears semantic and lexical error counters. No returned ownership.
 */
void compiler_context_init(const char *output_path);

/**
 * @brief Return the configured generated-C output path.
 *
 * @return Borrowed pointer to the path passed to compiler_context_init(); no
 *         ownership is transferred.
 */
const char *compiler_context_output_path(void);

/**
 * @brief Report and count a semantic error at the current source line.
 *
 * @param format Borrowed printf-style format string; must not be NULL.
 * @param ... Format arguments matching @p format.
 *
 * Increments the semantic error count. The caller should continue parsing when
 * possible so multiple errors can be reported. No returned ownership.
 */
void compiler_context_report_semantic_error(const char *format, ...);

/**
 * @brief Increment the lexical error count.
 *
 * Called by lexer.l immediately before returning LEXICAL_ERROR. No returned
 * ownership.
 */
void compiler_context_add_lexical_error(void);

/**
 * @brief Return total compilation errors tracked by the context.
 *
 * @return Number of semantic plus lexical errors. No ownership is returned.
 */
int compiler_context_error_count(void);

#endif
