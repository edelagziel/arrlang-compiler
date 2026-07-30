/**
 * @file statement_block.c
 * @brief Active-output redirection for nested ArrLang statement blocks.
 *
 * Responsibilities: allocate a StatementBlock, redirect codegen output into
 * its owned buffer while parsing the nested block, restore the previous active
 * destination, and free block storage. It does not decide when blocks are valid
 * or emit if/loop syntax.
 *
 * Main dependencies: codegen active-buffer API and StringBuffer storage.
 * Typical flow: parser sees `{`, begins a block, parses items into it, finishes
 * on `}`, and passes the block to codegen_emit_if() or codegen_emit_loop().
 */
#include "statement_block.h"

#include "codegen.h"

#include <stdio.h>
#include <stdlib.h>

StatementBlock *statement_block_begin(void)
{
    StatementBlock *block = malloc(sizeof(*block));

    if (block == NULL) {
        fprintf(stderr, "Out of memory while creating statement block.\n");
        exit(1);
    }

    /* Save the current destination so nested block parsing can be transparent
       to the outer statement stream. */
    string_buffer_init(&block->buffer);
    block->previous = codegen_set_active_buffer(&block->buffer);
    return block;
}

StatementBlock *statement_block_finish(StatementBlock *block)
{
    codegen_set_active_buffer(block->previous);
    block->previous = NULL;
    return block;
}

void statement_block_free(StatementBlock *block)
{
    if (block == NULL) {
        return;
    }

    string_buffer_free(&block->buffer);
    free(block);
}
