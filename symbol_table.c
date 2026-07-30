#include "symbol_table.h"

#include <stdlib.h>
#include <string.h>

static Symbol *symbols = NULL;
static size_t symbol_count = 0;
static size_t symbol_capacity = 0;

static char *copy_string(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, text, length);
    }

    return copy;
}

void symbol_table_init(void)
{
    symbols = NULL;
    symbol_count = 0;
    symbol_capacity = 0;
}

int symbol_table_insert(const char *name, SymbolKind kind, int array_size)
{
    Symbol *grown_symbols;

    if (symbol_table_lookup(name) != NULL) {
        return 0;
    }

    if (symbol_count == symbol_capacity) {
        size_t new_capacity = symbol_capacity == 0 ? 8 : symbol_capacity * 2;
        grown_symbols = realloc(symbols, new_capacity * sizeof(*symbols));

        if (grown_symbols == NULL) {
            return -1;
        }

        symbols = grown_symbols;
        symbol_capacity = new_capacity;
    }

    symbols[symbol_count].name = copy_string(name);
    if (symbols[symbol_count].name == NULL) {
        return -1;
    }

    symbols[symbol_count].kind = kind;
    symbols[symbol_count].array_size = array_size;
    symbol_count++;

    return 1;
}

Symbol *symbol_table_lookup(const char *name)
{
    size_t i;

    for (i = 0; i < symbol_count; i++) {
        if (strcmp(symbols[i].name, name) == 0) {
            return &symbols[i];
        }
    }

    return NULL;
}

void symbol_table_free(void)
{
    size_t i;

    for (i = 0; i < symbol_count; i++) {
        free(symbols[i].name);
    }

    free(symbols);
    symbols = NULL;
    symbol_count = 0;
    symbol_capacity = 0;
}
