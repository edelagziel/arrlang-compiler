# ArrLang Compiler

A source-to-source compiler for the ArrLang programming language.

The compiler accepts ArrLang source code and translates it into C, which can
then be compiled into an executable program.

## Project Layout

```text
src/        C implementation modules
include/    public module headers
grammar/    Flex and Bison grammar sources
examples/   example ArrLang programs
tests/      valid, invalid, and runtime test programs
build/      generated files, objects, binaries, and generated C output
```

Generated Flex/Bison files are written to `build/generated/`, object files to
`build/obj/`, the compiler executable to `build/bin/arrlangc`, and generated
example/test output to `build/output/`.

## Build And Run

Build the compiler:

```sh
make clean
make
```

Run the sample program:

```sh
make run
```

Run the test suite:

```sh
make test
```

Manual pipeline:

```sh
mkdir -p build/output

./build/bin/arrlangc \
  examples/sample.arr \
  build/output/sample.c

gcc -Wall -Wextra -std=c11 \
  build/output/sample.c \
  -o build/output/sample

./build/output/sample
```

Clean generated files:

```sh
make clean
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

## Submission Locations

For the Moodle ZIP, use these organized paths:

- lexer source: `grammar/lexer.l`
- parser source: `grammar/parser.y`
- Makefile: `Makefile`
- compiler executable after build: `build/bin/arrlangc`
- example ArrLang source: `examples/sample.arr`
- generated C sample after `make sample`: `build/output/sample.c`
- generated sample executable after `make sample`: `build/output/sample`
