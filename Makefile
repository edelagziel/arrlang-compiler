CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = arrlangc
OBJS = parser.tab.o lex.yy.o symbol_table.o

all: $(TARGET)

parser.tab.c parser.tab.h: parser.y
	bison -d parser.y

lex.yy.c: lexer.l parser.tab.h
	flex lexer.l

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

parser.tab.o: parser.tab.c symbol_table.h
lex.yy.o: lex.yy.c parser.tab.h
symbol_table.o: symbol_table.c symbol_table.h

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) parser.tab.c parser.tab.h lex.yy.c

.PHONY: all clean
