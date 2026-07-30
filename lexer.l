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
%}

%%

"scl"                           return SCL;
"arr"                           return ARR;

[0-9]+                          {
                                    yylval.num = atoi(yytext);
                                    return NUMBER;
                                }
[a-zA-Z_][a-zA-Z0-9_]*         {
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
"="                             return '=';
"("                             return '(';
")"                             return ')';
"+"                             return '+';
"-"                             return '-';
"*"                             return '*';
"/"                             return '/';

[ \t\r\n]+                      ;

. {
    fprintf(
        stderr,
        "Lexical error on line %d: unexpected character '%s'\n",
        yylineno,
        yytext
    );
}

%%
