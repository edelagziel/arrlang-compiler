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

## Build And Run

Build:

```sh
make
```

Compile an ArrLang file to C:

```sh
./arrlangc input.arr output.c
```

Compile and run the generated C:

```sh
gcc -Wall -Wextra -std=c11 output.c -o output
./output
```

Run the included sample:

```sh
mkdir -p build
./arrlangc sample.arr build/sample.c
gcc -Wall -Wextra -std=c11 build/sample.c -o build/sample
./build/sample
```

## Supported Features

ArrLang currently supports:

- scalar declarations: `scl x;`
- array declarations: `arr nums{4};`
- array literal assignment: `nums = [1, 2, 3, 4];`
- array operators: array/scalar arithmetic, array/array arithmetic, `#`, `!`, `:`, `~`, `$`
- scalar assignments: `x = 5 + 3 * 2;`
- scalar arithmetic with integer literals, scalar identifiers, `+`, `-`, `*`, `/`, parentheses, and unary minus
- print statements: `print "Results": x, nums;`
- if and if/else statements
- loop statements with scalar loop counts

Arrays cannot be indexed on the left side of an assignment.

Clean generated files:

```sh
make clean
```
