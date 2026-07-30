%code requires {
typedef struct Expression Expression;
typedef struct StringBuffer StringBuffer;
typedef struct StatementBlock StatementBlock;
}

%{
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"

typedef struct StringBuffer {
    char *data;
    size_t length;
    size_t capacity;
} StringBuffer;

typedef struct StatementBlock {
    StringBuffer buffer;
    StringBuffer *previous;
} StatementBlock;

typedef enum {
    EXPR_ERROR,
    EXPR_SCALAR,
    EXPR_ARRAY
} ExprKind;

typedef struct Expression {
    ExprKind kind;
    int array_size;
    char *value;
    StringBuffer setup;
} Expression;

int yylex(void);
void yyerror(const char *message);

extern FILE *yyin;
extern int yylineno;

int lexical_errors = 0;

static StringBuffer declarations;
static StringBuffer statements;
static StringBuffer *active_statements = NULL;
static const char *output_path = NULL;
static int semantic_errors = 0;
static int next_array_temp = 1;
static int next_scalar_temp = 1;
static int next_loop_index = 1;

static void buffer_init(StringBuffer *buffer);
static void buffer_free(StringBuffer *buffer);
static void buffer_appendf(StringBuffer *buffer, const char *format, ...);
static void buffer_append_buffer(StringBuffer *target, const StringBuffer *source);
static char *copy_string(const char *text);
static char *format_string(const char *format, ...);
static void report_semantic_error(const char *format, ...);
static Expression *expression_create(ExprKind kind, int array_size, const char *value);
static Expression *expression_error(void);
static void expression_free(Expression *expression);
static char *new_array_temp(int size);
static char *new_scalar_temp(void);
static char *new_loop_index(void);
static void emit_setup(const StringBuffer *setup);
static void declare_scalar(char *name);
static void declare_array(char *name, int size);
static void assign_expression(char *name, Expression *expression);
static void reverse_array(char *name);
static void sort_array(char *name);
static void begin_print(char *label);
static void print_separator(void);
static void print_expression(Expression *expression);
static void end_print(void);
static void emit_expression_statement(Expression *expression);
static StatementBlock *begin_statement_block(void);
static StatementBlock *finish_statement_block(StatementBlock *block);
static void statement_block_free(StatementBlock *block);
static void emit_if_statement(Expression *condition, StatementBlock *then_block, StatementBlock *else_block);
static void emit_loop_statement(Expression *count, StatementBlock *body);
static Expression *make_number_expression(int value);
static Expression *make_identifier_expression(char *name);
static Expression *make_array_literal_expression(Expression *elements);
static Expression *array_literal_start(int element);
static Expression *array_literal_append(Expression *literal, int element);
static Expression *make_parenthesized_expression(Expression *expression);
static Expression *make_unary_minus(Expression *expression);
static Expression *make_array_size_expression(Expression *expression);
static Expression *make_index_expression(Expression *array, Expression *index);
static Expression *make_binary_expression(Expression *left, const char *operator_text, Expression *right);
static Expression *make_concat_expression(Expression *left, Expression *right);
static int write_c_output(void);
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
        $$ = finish_statement_block($1);
    }
;

block_begin:
    '{'
    {
        $$ = begin_statement_block();
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
        assign_expression($1, $3);
    }
;

reverse_statement:
    '~' IDENTIFIER ';'
    {
        reverse_array($2);
    }
;

sort_statement:
    '$' IDENTIFIER ';'
    {
        sort_array($2);
    }
;

print_statement:
    PRINT STRING_LITERAL ':'
    {
        begin_print($2);
    }
    print_arguments ';'
    {
        end_print();
    }
;

expression_statement:
    expression ';'
    {
        emit_expression_statement($1);
    }
;

print_arguments:
    expression
    {
        print_expression($1);
    }
    | print_arguments ',' expression
    {
        print_separator();
        print_expression($3);
    }
;

if_statement:
    IF expression statement_block
    {
        emit_if_statement($2, $3, NULL);
    }
    | IF expression statement_block ELSE statement_block
    {
        emit_if_statement($2, $3, $5);
    }
;

loop_statement:
    LOOP expression statement_block
    {
        emit_loop_statement($2, $3);
    }
;

array_literal:
    '[' array_elements ']'
    {
        $$ = make_array_literal_expression($2);
    }
;

array_elements:
    array_element
    {
        $$ = array_literal_start($1);
    }
    | array_elements ',' array_element
    {
        $$ = array_literal_append($1, $3);
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
        $$ = make_number_expression($1);
    }
    | IDENTIFIER
    {
        $$ = make_identifier_expression($1);
    }
    | array_literal
    {
        $$ = $1;
    }
    | '(' expression ')'
    {
        $$ = make_parenthesized_expression($2);
    }
    | '-' expression %prec UMINUS
    {
        $$ = make_unary_minus($2);
    }
    | '!' expression
    {
        $$ = make_array_size_expression($2);
    }
    | expression ':' expression
    {
        $$ = make_index_expression($1, $3);
    }
    | expression '#' expression
    {
        $$ = make_concat_expression($1, $3);
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

static void buffer_append_buffer(StringBuffer *target, const StringBuffer *source)
{
    if (source->data != NULL) {
        buffer_appendf(target, "%s", source->data);
    }
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

static Expression *expression_create(ExprKind kind, int array_size, const char *value)
{
    Expression *expression = malloc(sizeof(*expression));

    if (expression == NULL) {
        fprintf(stderr, "Out of memory while creating expression.\n");
        exit(1);
    }

    expression->kind = kind;
    expression->array_size = array_size;
    expression->value = copy_string(value);
    buffer_init(&expression->setup);

    return expression;
}

static Expression *expression_error(void)
{
    return expression_create(EXPR_ERROR, 0, "0");
}

static void expression_free(Expression *expression)
{
    if (expression == NULL) {
        return;
    }

    free(expression->value);
    buffer_free(&expression->setup);
    free(expression);
}

static char *new_array_temp(int size)
{
    char *name = format_string("__arr_tmp_%d", next_array_temp++);

    buffer_appendf(&declarations, "    int %s[%d];\n", name, size);
    return name;
}

static char *new_scalar_temp(void)
{
    char *name = format_string("__scl_tmp_%d", next_scalar_temp++);

    buffer_appendf(&declarations, "    int %s;\n", name);
    return name;
}

static char *new_loop_index(void)
{
    char *name = format_string("__i_%d", next_loop_index++);

    buffer_appendf(&declarations, "    int %s;\n", name);
    return name;
}

static void emit_setup(const StringBuffer *setup)
{
    buffer_append_buffer(active_statements, setup);
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

static void assign_expression(char *name, Expression *expression)
{
    Symbol *symbol = symbol_table_lookup(name);
    char *index_name;

    if (symbol == NULL) {
        report_semantic_error("assignment to undeclared variable '%s'", name);
    } else if (expression->kind != EXPR_ERROR) {
        if (symbol->kind == SYMBOL_SCALAR && expression->kind != EXPR_SCALAR) {
            report_semantic_error("cannot assign an array expression to scalar '%s'", name);
        } else if (symbol->kind == SYMBOL_ARRAY && expression->kind != EXPR_ARRAY) {
            report_semantic_error("cannot assign a scalar expression to array '%s'", name);
        } else if (symbol->kind == SYMBOL_ARRAY && expression->array_size != symbol->array_size) {
            report_semantic_error(
                "array expression for '%s' has size %d but declared size is %d",
                name,
                expression->array_size,
                symbol->array_size
            );
        } else if (symbol->kind == SYMBOL_SCALAR) {
            emit_setup(&expression->setup);
            buffer_appendf(active_statements, "    %s = %s;\n", name, expression->value);
        } else {
            index_name = new_loop_index();
            emit_setup(&expression->setup);
            buffer_appendf(
                active_statements,
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

static void reverse_array(char *name)
{
    Symbol *symbol = symbol_table_lookup(name);
    char *index_name;
    char *temp_name;

    if (symbol == NULL) {
        report_semantic_error("reverse of undeclared variable '%s'", name);
    } else if (symbol->kind != SYMBOL_ARRAY) {
        report_semantic_error("reverse operator requires array operand '%s'", name);
    } else {
        index_name = new_loop_index();
        temp_name = new_scalar_temp();
        buffer_appendf(
            active_statements,
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

static void sort_array(char *name)
{
    Symbol *symbol = symbol_table_lookup(name);
    char *i_name;
    char *j_name;
    char *temp_name;

    if (symbol == NULL) {
        report_semantic_error("sort of undeclared variable '%s'", name);
    } else if (symbol->kind != SYMBOL_ARRAY) {
        report_semantic_error("sort operator requires array operand '%s'", name);
    } else {
        i_name = new_loop_index();
        j_name = new_loop_index();
        temp_name = new_scalar_temp();
        buffer_appendf(
            active_statements,
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

static void begin_print(char *label)
{
    if (strcmp(label, "\"\"") != 0) {
        buffer_appendf(active_statements, "    printf(\"%%s: \", %s);\n", label);
    }
    free(label);
}

static void print_separator(void)
{
    buffer_appendf(active_statements, "    printf(\", \");\n");
}

static void print_expression(Expression *expression)
{
    char *index_name;

    if (expression->kind == EXPR_ERROR) {
        expression_free(expression);
        return;
    }

    emit_setup(&expression->setup);

    if (expression->kind == EXPR_SCALAR) {
        buffer_appendf(active_statements, "    printf(\"%%d\", %s);\n", expression->value);
    } else {
        index_name = new_loop_index();
        buffer_appendf(
            active_statements,
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

static void end_print(void)
{
    buffer_appendf(active_statements, "    printf(\"\\n\");\n");
}

static void emit_expression_statement(Expression *expression)
{
    if (expression->kind != EXPR_ERROR) {
        emit_setup(&expression->setup);
        if (expression->kind == EXPR_SCALAR) {
            buffer_appendf(active_statements, "    (void)(%s);\n", expression->value);
        } else {
            buffer_appendf(active_statements, "    (void)%s;\n", expression->value);
        }
    }

    expression_free(expression);
}

static StatementBlock *begin_statement_block(void)
{
    StatementBlock *block = malloc(sizeof(*block));

    if (block == NULL) {
        fprintf(stderr, "Out of memory while creating statement block.\n");
        exit(1);
    }

    buffer_init(&block->buffer);
    block->previous = active_statements;
    active_statements = &block->buffer;
    return block;
}

static StatementBlock *finish_statement_block(StatementBlock *block)
{
    active_statements = block->previous;
    block->previous = NULL;
    return block;
}

static void statement_block_free(StatementBlock *block)
{
    if (block == NULL) {
        return;
    }

    buffer_free(&block->buffer);
    free(block);
}

static void emit_if_statement(Expression *condition, StatementBlock *then_block, StatementBlock *else_block)
{
    if (condition->kind == EXPR_ERROR) {
        expression_free(condition);
        statement_block_free(then_block);
        statement_block_free(else_block);
        return;
    }

    if (condition->kind != EXPR_SCALAR) {
        report_semantic_error("if condition must be a scalar expression");
    } else {
        emit_setup(&condition->setup);
        buffer_appendf(active_statements, "    if (%s) {\n", condition->value);
        buffer_append_buffer(active_statements, &then_block->buffer);
        buffer_appendf(active_statements, "    }");

        if (else_block != NULL) {
            buffer_appendf(active_statements, " else {\n");
            buffer_append_buffer(active_statements, &else_block->buffer);
            buffer_appendf(active_statements, "    }");
        }

        buffer_appendf(active_statements, "\n");
    }

    expression_free(condition);
    statement_block_free(then_block);
    statement_block_free(else_block);
}

static void emit_loop_statement(Expression *count, StatementBlock *body)
{
    char *index_name;
    char *count_name;

    if (count->kind == EXPR_ERROR) {
        expression_free(count);
        statement_block_free(body);
        return;
    }

    if (count->kind != EXPR_SCALAR) {
        report_semantic_error("loop count must be a scalar expression");
    } else {
        index_name = new_loop_index();
        count_name = new_scalar_temp();
        emit_setup(&count->setup);
        buffer_appendf(active_statements, "    %s = %s;\n", count_name, count->value);
        buffer_appendf(
            active_statements,
            "    for (%s = 0; %s < %s; %s++) {\n",
            index_name,
            index_name,
            count_name,
            index_name
        );
        buffer_append_buffer(active_statements, &body->buffer);
        buffer_appendf(active_statements, "    }\n");
        free(index_name);
        free(count_name);
    }

    expression_free(count);
    statement_block_free(body);
}

static Expression *make_number_expression(int value)
{
    char *text = format_string("%d", value);
    Expression *expression = expression_create(EXPR_SCALAR, 0, text);

    free(text);
    return expression;
}

static Expression *make_identifier_expression(char *name)
{
    Symbol *symbol = symbol_table_lookup(name);
    Expression *expression;

    if (symbol == NULL) {
        report_semantic_error("use of undeclared variable '%s'", name);
        expression = expression_error();
    } else if (symbol->kind == SYMBOL_SCALAR) {
        expression = expression_create(EXPR_SCALAR, 0, name);
    } else {
        expression = expression_create(EXPR_ARRAY, symbol->array_size, name);
    }

    free(name);
    return expression;
}

static Expression *make_array_literal_expression(Expression *elements)
{
    return elements;
}

static Expression *array_literal_start(int element)
{
    char *temp_name = new_array_temp(1);
    Expression *literal = expression_create(EXPR_ARRAY, 1, temp_name);

    buffer_appendf(&literal->setup, "    %s[0] = %d;\n", temp_name, element);
    free(temp_name);
    return literal;
}

static Expression *array_literal_append(Expression *literal, int element)
{
    char *old_name;
    char *new_name;
    char *index_name;
    Expression *expanded;

    if (literal->kind == EXPR_ERROR) {
        return literal;
    }

    old_name = copy_string(literal->value);
    new_name = new_array_temp(literal->array_size + 1);
    index_name = new_loop_index();
    expanded = expression_create(EXPR_ARRAY, literal->array_size + 1, new_name);
    buffer_append_buffer(&expanded->setup, &literal->setup);
    buffer_appendf(
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

static Expression *make_parenthesized_expression(Expression *expression)
{
    char *value;
    Expression *result;

    if (expression->kind != EXPR_SCALAR) {
        return expression;
    }

    value = format_string("(%s)", expression->value);
    result = expression_create(EXPR_SCALAR, 0, value);
    buffer_append_buffer(&result->setup, &expression->setup);
    expression_free(expression);
    free(value);
    return result;
}

static Expression *make_unary_minus(Expression *expression)
{
    char *value;
    Expression *result;

    if (expression->kind == EXPR_ERROR) {
        return expression;
    }

    if (expression->kind != EXPR_SCALAR) {
        report_semantic_error("unary minus requires a scalar expression");
        expression_free(expression);
        return expression_error();
    }

    value = format_string("(-%s)", expression->value);
    result = expression_create(EXPR_SCALAR, 0, value);
    buffer_append_buffer(&result->setup, &expression->setup);
    expression_free(expression);
    free(value);
    return result;
}

static Expression *make_array_size_expression(Expression *expression)
{
    char *value;
    Expression *result;

    if (expression->kind == EXPR_ERROR) {
        return expression;
    }

    if (expression->kind != EXPR_ARRAY) {
        report_semantic_error("size operator requires an array expression");
        expression_free(expression);
        return expression_error();
    }

    value = format_string("%d", expression->array_size);
    result = expression_create(EXPR_SCALAR, 0, value);
    buffer_append_buffer(&result->setup, &expression->setup);
    expression_free(expression);
    free(value);
    return result;
}

static Expression *make_index_expression(Expression *array, Expression *index)
{
    char *temp_name;
    Expression *result;

    if (array->kind == EXPR_ERROR || index->kind == EXPR_ERROR) {
        expression_free(array);
        expression_free(index);
        return expression_error();
    }

    if (array->kind != EXPR_ARRAY || index->kind != EXPR_SCALAR) {
        report_semantic_error("index operator requires array on the left and scalar on the right");
        expression_free(array);
        expression_free(index);
        return expression_error();
    }

    temp_name = new_scalar_temp();
    result = expression_create(EXPR_SCALAR, 0, temp_name);
    buffer_append_buffer(&result->setup, &array->setup);
    buffer_append_buffer(&result->setup, &index->setup);
    buffer_appendf(
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

static Expression *make_binary_expression(Expression *left, const char *operator_text, Expression *right)
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

    if (left->kind == EXPR_SCALAR && right->kind == EXPR_SCALAR) {
        if (strcmp(operator_text, "/") == 0) {
            temp_name = new_scalar_temp();
            result = expression_create(EXPR_SCALAR, 0, temp_name);
            buffer_append_buffer(&result->setup, &left->setup);
            buffer_append_buffer(&result->setup, &right->setup);
            buffer_appendf(
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
            buffer_append_buffer(&result->setup, &left->setup);
            buffer_append_buffer(&result->setup, &right->setup);
            free(value);
        }
    } else if (left->kind == EXPR_ARRAY && right->kind == EXPR_ARRAY) {
        if (left->array_size != right->array_size) {
            report_semantic_error("array operands must have the same size");
            expression_free(left);
            expression_free(right);
            return expression_error();
        }

        temp_name = new_array_temp(left->array_size);
        index_name = new_loop_index();
        result = expression_create(EXPR_ARRAY, left->array_size, temp_name);
        buffer_append_buffer(&result->setup, &left->setup);
        buffer_append_buffer(&result->setup, &right->setup);
        if (strcmp(operator_text, "/") == 0) {
            buffer_appendf(
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
            buffer_appendf(
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
        temp_name = new_array_temp(left->array_size);
        index_name = new_loop_index();
        result = expression_create(EXPR_ARRAY, left->array_size, temp_name);
        buffer_append_buffer(&result->setup, &left->setup);
        buffer_append_buffer(&result->setup, &right->setup);
        if (strcmp(operator_text, "/") == 0) {
            buffer_appendf(
                &result->setup,
                "    if (%s == 0) {\n"
                "        fprintf(stderr, \"Runtime error: division by zero\\n\");\n"
                "        return 1;\n"
                "    }\n",
                right->value
            );
        }
        buffer_appendf(
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
        report_semantic_error("scalar-array arithmetic requires the array operand on the left");
        expression_free(left);
        expression_free(right);
        return expression_error();
    }

    expression_free(left);
    expression_free(right);
    return result;
}

static Expression *make_concat_expression(Expression *left, Expression *right)
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
        report_semantic_error("concatenation requires array operands");
        expression_free(left);
        expression_free(right);
        return expression_error();
    }

    result_size = left->array_size + right->array_size;
    temp_name = new_array_temp(result_size);
    index_name = new_loop_index();
    result = expression_create(EXPR_ARRAY, result_size, temp_name);
    buffer_append_buffer(&result->setup, &left->setup);
    buffer_append_buffer(&result->setup, &right->setup);
    buffer_appendf(
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
    active_statements = &statements;

    parse_result = yyparse();
    fclose(input);

    if (parse_result != 0 || semantic_errors > 0 || lexical_errors > 0) {
        result = 1;
    } else {
        result = write_c_output();
    }

    buffer_free(&declarations);
    buffer_free(&statements);
    symbol_table_free();

    return result;
}
