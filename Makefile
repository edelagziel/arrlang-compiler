CC := gcc
CFLAGS := -Wall -Wextra -std=c11

SRC_DIR := src
INC_DIR := include
GRAMMAR_DIR := grammar
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
GEN_DIR := $(BUILD_DIR)/generated
BIN_DIR := $(BUILD_DIR)/bin
OUTPUT_DIR := $(BUILD_DIR)/output

TARGET := $(BIN_DIR)/arrlangc

SRC_FILES := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/symbol_table.c \
	$(SRC_DIR)/string_buffer.c \
	$(SRC_DIR)/compiler_context.c \
	$(SRC_DIR)/expression.c \
	$(SRC_DIR)/codegen.c \
	$(SRC_DIR)/statement_block.c

OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES)) \
	$(OBJ_DIR)/parser.tab.o \
	$(OBJ_DIR)/lex.yy.o

CPPFLAGS := -I$(INC_DIR) -I$(GEN_DIR)

all: $(TARGET)

compiler: $(TARGET)

$(BUILD_DIR) $(OBJ_DIR) $(GEN_DIR) $(BIN_DIR) $(OUTPUT_DIR):
	mkdir -p $@

$(GEN_DIR)/parser.tab.c $(GEN_DIR)/parser.tab.h: $(GRAMMAR_DIR)/parser.y | $(GEN_DIR)
	bison -d -o $(GEN_DIR)/parser.tab.c --defines=$(GEN_DIR)/parser.tab.h $(GRAMMAR_DIR)/parser.y

$(GEN_DIR)/lex.yy.c: $(GRAMMAR_DIR)/lexer.l $(GEN_DIR)/parser.tab.h | $(GEN_DIR)
	flex -o $(GEN_DIR)/lex.yy.c $(GRAMMAR_DIR)/lexer.l

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR) $(GEN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/parser.tab.o: $(GEN_DIR)/parser.tab.c $(GEN_DIR)/parser.tab.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(GEN_DIR)/parser.tab.c -o $@

$(OBJ_DIR)/lex.yy.o: $(GEN_DIR)/lex.yy.c $(GEN_DIR)/parser.tab.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(GEN_DIR)/lex.yy.c -o $@

$(TARGET): $(OBJ_FILES) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJ_FILES) -o $(TARGET)

sample: $(TARGET) | $(OUTPUT_DIR)
	$(TARGET) examples/sample.arr $(OUTPUT_DIR)/sample.c
	$(CC) $(CFLAGS) $(OUTPUT_DIR)/sample.c -o $(OUTPUT_DIR)/sample

run: sample
	$(OUTPUT_DIR)/sample

test: $(TARGET) | $(OUTPUT_DIR)
	@set -e; \
	for test in tests/valid_*.arr; do \
		name=$$(basename "$$test" .arr); \
		echo "VALID:$$name"; \
		$(TARGET) "$$test" "$(OUTPUT_DIR)/$$name.c"; \
		$(CC) $(CFLAGS) "$(OUTPUT_DIR)/$$name.c" -o "$(OUTPUT_DIR)/$$name"; \
		"$(OUTPUT_DIR)/$$name" > "$(OUTPUT_DIR)/$$name.out"; \
	done; \
	for test in tests/invalid_*.arr; do \
		name=$$(basename "$$test" .arr); \
		echo "INVALID:$$name"; \
		if $(TARGET) "$$test" "$(OUTPUT_DIR)/$$name.c" > "$(OUTPUT_DIR)/$$name.out" 2> "$(OUTPUT_DIR)/$$name.err"; then \
			echo "UNEXPECTED_SUCCESS:$$name"; \
			exit 1; \
		else \
			echo "EXPECTED_FAILURE:$$name"; \
		fi; \
	done; \
	echo "RUNTIME:runtime_index_oob"; \
	$(TARGET) tests/runtime_index_oob.arr "$(OUTPUT_DIR)/runtime_index_oob.c"; \
	$(CC) $(CFLAGS) "$(OUTPUT_DIR)/runtime_index_oob.c" -o "$(OUTPUT_DIR)/runtime_index_oob"; \
	if "$(OUTPUT_DIR)/runtime_index_oob" > "$(OUTPUT_DIR)/runtime_index_oob.out" 2> "$(OUTPUT_DIR)/runtime_index_oob.err"; then \
		echo "UNEXPECTED_RUNTIME_SUCCESS"; \
		exit 1; \
	else \
		echo "EXPECTED_RUNTIME_FAILURE"; \
	fi

clean:
	rm -rf $(BUILD_DIR)
	rm -f arrlangc *.o lex.yy.c parser.tab.c parser.tab.h output output.c

.PHONY: all compiler sample run test clean
