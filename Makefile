CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = arrlangc
OBJS = main.o parser.tab.o lex.yy.o symbol_table.o string_buffer.o compiler_context.o expression.o codegen.o statement_block.o

all: $(TARGET)

parser.tab.c parser.tab.h: parser.y
	bison -d parser.y

lex.yy.c: lexer.l parser.tab.h
	flex lexer.l

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c codegen.h compiler_context.h symbol_table.h
parser.tab.o: parser.tab.c codegen.h expression.h statement_block.h string_buffer.h symbol_table.h
lex.yy.o: lex.yy.c parser.tab.h compiler_context.h
symbol_table.o: symbol_table.c symbol_table.h
string_buffer.o: string_buffer.c string_buffer.h
compiler_context.o: compiler_context.c compiler_context.h
expression.o: expression.c expression.h codegen.h compiler_context.h symbol_table.h string_buffer.h
codegen.o: codegen.c codegen.h compiler_context.h expression.h statement_block.h symbol_table.h string_buffer.h
statement_block.o: statement_block.c statement_block.h codegen.h string_buffer.h

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) parser.tab.c parser.tab.h lex.yy.c

.PHONY: all clean
