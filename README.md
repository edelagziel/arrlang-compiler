# ArrLang Compiler

A source-to-source compiler for the ArrLang programming language.

The compiler accepts ArrLang source code and translates it into C or C++ code, which can then be compiled into an executable program.

The project is implemented using:

- Flex / Lex for lexical analysis
- Bison / YACC for parsing
- C or C++ for semantic analysis and code generation
- GCC or another C/C++ compiler for compiling the generated output

---

## Project Goal

The goal of this project is to build a compiler for ArrLang, a small programming language focused on scalar and array operations.

The compilation pipeline is:

```text
ArrLang source code
        ↓
Flex lexer
        ↓
Tokens
        ↓
Bison parser
        ↓
Semantic analysis
        ↓
Generated C/C++ code
        ↓
C/C++ compiler
        ↓
Executable program