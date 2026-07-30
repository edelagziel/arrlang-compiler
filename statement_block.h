/**
 * @file statement_block.h
 * @brief Nested statement-buffer handling for if/else and loop bodies.
 *
 * This module temporarily redirects code generation into a private
 * StringBuffer while parsing a nested ArrLang block. It does not parse block
 * contents or emit if/loop syntax itself; codegen consumes the finished block.
 *
 * Main dependencies: string_buffer for owned text storage and codegen for the
 * active output destination.
 *
 * Typical usage:
 * @code
 * StatementBlock *block = statement_block_begin();
 * // Parser emits statements into block->buffer.
 * block = statement_block_finish(block);
 * codegen_emit_loop(count_expression, block); // consumes block
 * @endcode
 */
#ifndef STATEMENT_BLOCK_H
#define STATEMENT_BLOCK_H

#include "string_buffer.h"

/**
 * @brief Captured C statements for a nested ArrLang block.
 *
 * @var StatementBlock::buffer
 * Owned StringBuffer that receives statements while the block is active.
 *
 * @var StatementBlock::previous
 * Borrowed pointer to the codegen active buffer that should be restored by
 * statement_block_finish(). It is NULL after finish.
 *
 * Valid state: begin returns an active block, finish returns an inactive block
 * ready for codegen consumption. statement_block_free() must eventually be
 * called, usually by codegen_emit_if() or codegen_emit_loop().
 */
typedef struct StatementBlock {
    StringBuffer buffer;
    StringBuffer *previous;
} StatementBlock;

/**
 * @brief Start capturing statements into a new nested block.
 *
 * @return Caller owns returned StatementBlock*. The active codegen destination
 *         is redirected until statement_block_finish() is called.
 */
StatementBlock *statement_block_begin(void);

/**
 * @brief Finish a nested block and restore the previous active output buffer.
 *
 * @param block Ownership remains with caller; must not be NULL and must have
 *        been returned by statement_block_begin().
 * @return Same borrowed/owned block pointer, now inactive and ready to pass to
 *         codegen.
 */
StatementBlock *statement_block_finish(StatementBlock *block);

/**
 * @brief Free a statement block and its owned buffer.
 *
 * @param block Ownership transferred; may be NULL. Usually called by codegen
 *        functions that consume statement blocks.
 */
void statement_block_free(StatementBlock *block);

#endif
