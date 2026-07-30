/**
 * @file parser.y
 * @brief Bison grammar for ArrLang with delegating semantic actions.
 *
 * Responsibilities: define tokens, precedence, grammar rules, semantic value
 * types, and Bison destructors. Semantic actions are intentionally small and
 * delegate expression construction, statement-block handling, and code
 * generation to focused modules. This file does not implement StringBuffer,
 * symbol-table storage, CLI handling, or large code-generation routines.
 *
 * Main dependencies: expression.h, statement_block.h, and codegen.h. Typical
 * flow: yyparse() recognizes one top-level block and delegates each declaration,
 * statement, or expression rule to the appropriate module.
 */
%code requires {
#include "expression.h"
#include "statement_block.h"
}

%{
#include <stdlib.h>

#include "codegen.h"
#include "expression.h"
#include "statement_block.h"

int yylex(void);
void yyerror(const char *message);
%}

%union {
    int num;
    char *str;
    Expression *expression;
    StatementBlock *statement_block;
}

%token SCL
%token ARR
%token PRINT
%token IF
%token ELSE
%token LOOP
%token LEXICAL_ERROR
%token <str> IDENTIFIER
%token <str> STRING_LITERAL
%token <num> NUMBER

%type <expression> expression array_literal array_elements
%type <num> array_element
%type <statement_block> block_begin statement_block
%destructor { free($$); } <str>
%destructor { expression_free($$); } <expression>
%destructor { statement_block_free($$); } <statement_block>

%left ':'
%left '+' '-'
%left '*' '/'
%left '#'
%right '!'
%right UMINUS

%start program

%%

program:
    block
;

block:
    '{' item_list '}'
;

statement_block:
    block_begin item_list '}'
    {
        $$ = statement_block_finish($1);
    }
;

block_begin:
    '{'
    {
        $$ = statement_block_begin();
    }
;

item_list:
    /* empty */
    | item_list item
;

item:
    declaration
    | assignment
    | reverse_statement
    | sort_statement
    | print_statement
    | if_statement
    | loop_statement
    | expression_statement
;

declaration:
    SCL IDENTIFIER ';'
    {
        codegen_declare_scalar($2);
    }
    | ARR IDENTIFIER '{' NUMBER '}' ';'
    {
        codegen_declare_array($2, $4);
    }
;

assignment:
    IDENTIFIER '=' expression ';'
    {
        codegen_assign_expression($1, $3);
    }
;

reverse_statement:
    '~' IDENTIFIER ';'
    {
        codegen_reverse_array($2);
    }
;

sort_statement:
    '$' IDENTIFIER ';'
    {
        codegen_sort_array($2);
    }
;

print_statement:
    PRINT STRING_LITERAL ':'
    {
        codegen_begin_print($2);
    }
    print_arguments ';'
    {
        codegen_end_print();
    }
;

print_arguments:
    expression
    {
        codegen_print_expression($1);
    }
    | print_arguments ',' expression
    {
        codegen_print_separator();
        codegen_print_expression($3);
    }
;

expression_statement:
    expression ';'
    {
        expression_emit_statement($1);
    }
;

if_statement:
    IF expression statement_block
    {
        codegen_emit_if($2, $3, NULL);
    }
    | IF expression statement_block ELSE statement_block
    {
        codegen_emit_if($2, $3, $5);
    }
;

loop_statement:
    LOOP expression statement_block
    {
        codegen_emit_loop($2, $3);
    }
;

array_literal:
    '[' array_elements ']'
    {
        $$ = expression_make_array_literal($2);
    }
;

array_elements:
    array_element
    {
        $$ = expression_array_literal_start($1);
    }
    | array_elements ',' array_element
    {
        $$ = expression_array_literal_append($1, $3);
    }
;

array_element:
    NUMBER
    {
        $$ = $1;
    }
    | '-' NUMBER
    {
        $$ = -$2;
    }
;

expression:
    NUMBER
    {
        $$ = expression_make_number($1);
    }
    | IDENTIFIER
    {
        $$ = expression_make_identifier($1);
    }
    | array_literal
    {
        $$ = $1;
    }
    | '(' expression ')'
    {
        $$ = expression_make_parenthesized($2);
    }
    | '-' expression %prec UMINUS
    {
        $$ = expression_make_unary_minus($2);
    }
    | '!' expression
    {
        $$ = expression_make_array_size($2);
    }
    | expression ':' expression
    {
        $$ = expression_make_index($1, $3);
    }
    | expression '#' expression
    {
        $$ = expression_make_concat($1, $3);
    }
    | expression '+' expression
    {
        $$ = expression_make_binary($1, "+", $3);
    }
    | expression '-' expression
    {
        $$ = expression_make_binary($1, "-", $3);
    }
    | expression '*' expression
    {
        $$ = expression_make_binary($1, "*", $3);
    }
    | expression '/' expression
    {
        $$ = expression_make_binary($1, "/", $3);
    }
;

%%
