/**
 * @file symbol_table.h
 * @brief Declaration table for ArrLang variables.
 *
 * This module records declared scalar and array variables, rejects duplicate
 * declarations, and lets semantic checks query variable kind and array size.
 * It does not parse declarations, emit C code, or validate expression
 * combinations beyond duplicate-name lookup.
 *
 * Main dependencies: C allocation and string comparison in the implementation.
 *
 * Typical usage:
 * @code
 * symbol_table_init();
 * symbol_table_insert("x", SYMBOL_SCALAR, 0);
 * Symbol *symbol = symbol_table_lookup("x"); // borrowed pointer
 * symbol_table_free();
 * @endcode
 */
#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

/**
 * @brief Kind of a declared ArrLang variable.
 */
typedef enum {
    SYMBOL_SCALAR, /**< Integer scalar declared with `scl name;`. */
    SYMBOL_ARRAY   /**< Integer array declared with `arr name{size};`. */
} SymbolKind;

/**
 * @brief One declared ArrLang variable.
 *
 * @var Symbol::name
 * Heap-owned copy of the variable name. Owned by the symbol table and freed by
 * symbol_table_free().
 *
 * @var Symbol::kind
 * Whether the declaration is scalar or array.
 *
 * @var Symbol::array_size
 * Positive declared array size for SYMBOL_ARRAY. Zero for SYMBOL_SCALAR.
 *
 * Valid state: entries returned by symbol_table_lookup() remain valid until
 * symbol_table_free() or until the table is otherwise modified by insertion.
 */
typedef struct {
    char *name;
    SymbolKind kind;
    int array_size;
} Symbol;

/**
 * @brief Initialize an empty symbol table.
 *
 * Must be called before insertion or lookup. No returned ownership.
 */
void symbol_table_init(void);

/**
 * @brief Insert a new symbol if the name is not already declared.
 *
 * @param name Borrowed pointer; must not be NULL. The name is copied
 *        internally.
 * @param kind Variable kind to store.
 * @param array_size Positive array size for arrays; ignored by scalars.
 *
 * @return 1 on successful insertion, 0 for duplicate declaration, -1 on
 *         allocation failure. No returned ownership.
 */
int symbol_table_insert(const char *name, SymbolKind kind, int array_size);

/**
 * @brief Find a declared symbol by name.
 *
 * @param name Borrowed pointer; must not be NULL.
 *
 * @return Borrowed pointer to the table entry, or NULL if not found. The caller
 *         must not free or modify the returned symbol.
 */
Symbol *symbol_table_lookup(const char *name);

/**
 * @brief Free all memory owned by the symbol table.
 *
 * Invalidates all borrowed Symbol pointers returned by symbol_table_lookup().
 * No returned ownership.
 */
void symbol_table_free(void);

#endif
