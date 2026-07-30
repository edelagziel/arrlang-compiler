#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

typedef enum {
    SYMBOL_SCALAR,
    SYMBOL_ARRAY
} SymbolKind;

typedef struct {
    char *name;
    SymbolKind kind;
    int array_size;
} Symbol;

void symbol_table_init(void);
int symbol_table_insert(const char *name, SymbolKind kind, int array_size);
Symbol *symbol_table_lookup(const char *name);
void symbol_table_free(void);

#endif
