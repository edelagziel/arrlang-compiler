%option noyywrap
%option yylineno
%option noinput
%option nounput

%top{
#define _POSIX_C_SOURCE 200809L
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

static char *copy_text(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, text, length);
    }

    return copy;
}

static int identifier_has_valid_tail(const char *text)
{
    size_t i;

    for (i = 1; text[i] != '\0'; i++) {
        if (!((text[i] >= 'a' && text[i] <= 'z') ||
              (text[i] >= 'A' && text[i] <= 'Z') ||
              (text[i] >= '0' && text[i] <= '9'))) {
            return 0;
        }
    }

    return 1;
}
%}

%%

"scl"                           return SCL;
"arr"                           return ARR;
"print"                         return PRINT;
"if"                            return IF;
"else"                          return ELSE;
"loop"                          return LOOP;

[0-9]+                          {
                                    yylval.num = atoi(yytext);
                                    return NUMBER;
                                }
\"([^\\\"\n]|\\.)*\"            {
                                    yylval.str = copy_text(yytext);
                                    if (yylval.str == NULL) {
                                        fprintf(stderr, "Out of memory while reading string literal.\n");
                                        exit(1);
                                    }
                                    return STRING_LITERAL;
                                }
\"([^\\\"\n]|\\.)*              {
                                    extern int lexical_errors;
                                    fprintf(stderr, "Lexical error on line %d: unterminated string literal\n", yylineno);
                                    lexical_errors++;
                                    return LEXICAL_ERROR;
                                }
[a-zA-Z_][a-zA-Z0-9_]*         {
                                    extern int lexical_errors;
                                    if (strlen(yytext) > 32) {
                                        fprintf(stderr, "Lexical error on line %d: identifier '%s' is longer than 32 characters\n", yylineno, yytext);
                                        lexical_errors++;
                                        return LEXICAL_ERROR;
                                    }
                                    if (!identifier_has_valid_tail(yytext)) {
                                        fprintf(stderr, "Lexical error on line %d: identifier '%s' contains a non-alphanumeric character after the first character\n", yylineno, yytext);
                                        lexical_errors++;
                                        return LEXICAL_ERROR;
                                    }
                                    yylval.str = copy_text(yytext);
                                    if (yylval.str == NULL) {
                                        fprintf(stderr, "Out of memory while reading identifier.\n");
                                        exit(1);
                                    }
                                    return IDENTIFIER;
                                }

"{"                             return '{';
"}"                             return '}';
";"                             return ';';
"["                             return '[';
"]"                             return ']';
","                             return ',';
"="                             return '=';
"("                             return '(';
")"                             return ')';
"+"                             return '+';
"-"                             return '-';
"*"                             return '*';
"/"                             return '/';
"#"                             return '#';
"!"                             return '!';
"~"                             return '~';
"$"                             return '$';
":"                             return ':';

[ \t\r\n]+                      ;

. {
    extern int lexical_errors;
    fprintf(
        stderr,
        "Lexical error on line %d: unexpected character '%s'\n",
        yylineno,
        yytext
    );
    lexical_errors++;
    return LEXICAL_ERROR;
}

%%
