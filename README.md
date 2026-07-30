# ArrLang Compiler

A source-to-source compiler for the ArrLang programming language.

The compiler accepts ArrLang source code and translates it into C, which can
then be compiled into an executable program.

The project is implemented using:

- Flex / Lex for lexical analysis
- Bison / YACC for parsing
- C for semantic analysis and code generation
- GCC for compiling the generated output

## Project Goal

The goal of this project is to build a compiler for ArrLang, a small language
focused on scalar and array operations.

The compilation pipeline is:

```text
ArrLang source code
        |
Flex lexer
        |
Tokens
        |
Bison parser
        |
Semantic analysis
        |
Generated C code
        |
C compiler
        |
Executable program
```

## Current Compiler Slice

Build:

```sh
make
```

Compile an ArrLang file to C:

```sh
./arrlangc input.arr output.c
```

This slice supports:

- scalar declarations: `scl x;`
- array declarations only: `arr nums{4};`
- array literal assignment: `nums = [1, 2, 3, 4];`
- scalar assignments: `x = 5 + 3 * 2;`
- scalar arithmetic with integer literals, scalar identifiers, `+`, `-`, `*`, `/`, parentheses, and unary minus

Arrays cannot be used in scalar expressions yet.

Clean generated files:

```sh
make clean
```
