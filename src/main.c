/**
 * @file main.c
 * @brief Command-line entry point for the ArrLang compiler.
 *
 * Responsibilities: validate CLI arguments, open the input file, initialize
 * compiler modules, call yyparse(), write output only if there are no syntax,
 * semantic, or lexical errors, and clean up module-owned memory. This file does
 * not implement grammar actions, lexical rules, symbol lookup, or code
 * generation details.
 *
 * Main dependencies: parser yyparse()/yyin, compiler_context, codegen, and
 * symbol_table. Typical flow is exactly the compiler lifecycle documented in
 * compiler_context.h.
 */
#include "codegen.h"
#include "compiler_context.h"
#include "symbol_table.h"

#include <stdio.h>

int yyparse(void);
extern FILE *yyin;
extern int yylineno;

void yyerror(const char *message)
{
    fprintf(stderr, "Syntax error on line %d: %s\n", yylineno, message);
}

int main(int argc, char **argv)
{
    FILE *input;
    int parse_result;
    int result = 0;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s input.arr output.c\n", argv[0]);
        return 1;
    }

    input = fopen(argv[1], "r");
    if (input == NULL) {
        perror(argv[1]);
        return 1;
    }

    yyin = input;
    compiler_context_init(argv[2]);
    symbol_table_init();
    codegen_init();

    parse_result = yyparse();
    fclose(input);

    if (parse_result != 0 || compiler_context_error_count() > 0) {
        result = 1;
    } else {
        result = codegen_write_output();
    }

    codegen_free();
    symbol_table_free();

    return result;
}
