%{
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuffer;

int yylex(void);
void yyerror(const char *message);

extern FILE *yyin;
extern int yylineno;

static StringBuffer declarations;
static StringBuffer statements;
static const char *output_path = NULL;
static int semantic_errors = 0;

static void buffer_init(StringBuffer *buffer);
static void buffer_free(StringBuffer *buffer);
static void buffer_appendf(StringBuffer *buffer, const char *format, ...);
static char *copy_string(const char *text);
static char *format_string(const char *format, ...);
static char *make_binary_expression(char *left, const char *operator_text, char *right);
static char *make_unary_expression(char *expression);
static void report_semantic_error(const char *format, ...);
static void declare_scalar(char *name);
static void declare_array(char *name, int size);
static void assign_scalar(char *name, char *expression);
static char *use_identifier(char *name);
static int write_c_output(void);
%}

%union {
    int num;
    char *str;
}

%token SCL
%token ARR
%token <str> IDENTIFIER
%token <num> NUMBER

%type <str> expression

%left '+' '-'
%left '*' '/'
%right UMINUS

%start program

%%

program:
    block
;

block:
    '{' item_list '}'
;

item_list:
    /* empty */
    | item_list item
;

item:
    declaration
    | assignment
;

declaration:
    SCL IDENTIFIER ';'
    {
        declare_scalar($2);
    }
    | ARR IDENTIFIER '{' NUMBER '}' ';'
    {
        declare_array($2, $4);
    }
;

assignment:
    IDENTIFIER '=' expression ';'
    {
        assign_scalar($1, $3);
    }
;

expression:
    NUMBER
    {
        $$ = format_string("%d", $1);
    }
    | IDENTIFIER
    {
        $$ = use_identifier($1);
    }
    | '(' expression ')'
    {
        $$ = format_string("(%s)", $2);
        free($2);
    }
    | '-' expression %prec UMINUS
    {
        $$ = make_unary_expression($2);
    }
    | expression '+' expression
    {
        $$ = make_binary_expression($1, "+", $3);
    }
    | expression '-' expression
    {
        $$ = make_binary_expression($1, "-", $3);
    }
    | expression '*' expression
    {
        $$ = make_binary_expression($1, "*", $3);
    }
    | expression '/' expression
    {
        $$ = make_binary_expression($1, "/", $3);
    }
;

%%

static void buffer_init(StringBuffer *buffer)
{
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

static void buffer_free(StringBuffer *buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

static void buffer_appendf(StringBuffer *buffer, const char *format, ...)
{
    va_list args;
    va_list args_copy;
    int required;
    size_t available;
    char *grown_data;

    va_start(args, format);
    va_copy(args_copy, args);
    required = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (required < 0) {
        fprintf(stderr, "Internal error while formatting generated code.\n");
        exit(1);
    }

    available = buffer->capacity > buffer->length ? buffer->capacity - buffer->length : 0;
    if ((size_t)required + 1 > available) {
        size_t new_capacity = buffer->capacity == 0 ? 128 : buffer->capacity;

        while ((size_t)required + 1 > new_capacity - buffer->length) {
            new_capacity *= 2;
        }

        grown_data = realloc(buffer->data, new_capacity);
        if (grown_data == NULL) {
            fprintf(stderr, "Out of memory while building generated code.\n");
            exit(1);
        }

        buffer->data = grown_data;
        buffer->capacity = new_capacity;
    }

    vsnprintf(buffer->data + buffer->length, buffer->capacity - buffer->length, format, args);
    va_end(args);
    buffer->length += (size_t)required;
}

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

static char *make_binary_expression(char *left, const char *operator_text, char *right)
{
    char *result = format_string("(%s %s %s)", left, operator_text, right);

    free(left);
    free(right);

    return result;
}

static char *make_unary_expression(char *expression)
{
    char *result = format_string("(-%s)", expression);

    free(expression);

    return result;
}

static void report_semantic_error(const char *format, ...)
{
    va_list args;

    fprintf(stderr, "Semantic error on line %d: ", yylineno);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
    semantic_errors++;
}

static void declare_scalar(char *name)
{
    int result = symbol_table_insert(name, SYMBOL_SCALAR, 0);

    if (result == 0) {
        report_semantic_error("duplicate declaration of '%s'", name);
    } else if (result < 0) {
        fprintf(stderr, "Out of memory while declaring '%s'.\n", name);
        exit(1);
    } else {
        buffer_appendf(&declarations, "    int %s;\n", name);
    }

    free(name);
}

static void declare_array(char *name, int size)
{
    int result;

    if (size <= 0) {
        report_semantic_error("array '%s' must have a positive size", name);
        free(name);
        return;
    }

    result = symbol_table_insert(name, SYMBOL_ARRAY, size);
    if (result == 0) {
        report_semantic_error("duplicate declaration of '%s'", name);
    } else if (result < 0) {
        fprintf(stderr, "Out of memory while declaring '%s'.\n", name);
        exit(1);
    } else {
        buffer_appendf(&declarations, "    int %s[%d];\n", name, size);
    }

    free(name);
}

static void assign_scalar(char *name, char *expression)
{
    Symbol *symbol = symbol_table_lookup(name);

    if (symbol == NULL) {
        report_semantic_error("assignment to undeclared variable '%s'", name);
    } else if (symbol->kind == SYMBOL_ARRAY) {
        report_semantic_error("cannot assign a scalar expression to array '%s'", name);
    } else {
        buffer_appendf(&statements, "    %s = %s;\n", name, expression);
    }

    free(name);
    free(expression);
}

static char *use_identifier(char *name)
{
    Symbol *symbol = symbol_table_lookup(name);
    char *result = copy_string(name);

    if (symbol == NULL) {
        report_semantic_error("use of undeclared variable '%s'", name);
    } else if (symbol->kind == SYMBOL_ARRAY) {
        report_semantic_error("array '%s' cannot be used as a scalar expression", name);
    }

    free(name);
    return result;
}

static int write_c_output(void)
{
    FILE *output = fopen(output_path, "w");

    if (output == NULL) {
        perror(output_path);
        return 1;
    }

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

    output_path = argv[2];
    yyin = input;

    symbol_table_init();
    buffer_init(&declarations);
    buffer_init(&statements);

    parse_result = yyparse();
    fclose(input);

    if (parse_result != 0 || semantic_errors > 0) {
        result = 1;
    } else {
        result = write_c_output();
    }

    buffer_free(&declarations);
    buffer_free(&statements);
    symbol_table_free();

    return result;
}
