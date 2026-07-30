/**
 * @file string_buffer.h
 * @brief Growable text buffer used by ArrLang code generation.
 *
 * This module owns a small dynamically resized, null-terminated character
 * buffer. It is responsible for initialization, formatted appends, copying
 * text from another buffer, and cleanup. It does not understand ArrLang
 * syntax, semantic types, or C code structure; callers decide what text to
 * append.
 *
 * Main dependencies: the C standard library for allocation and formatting.
 *
 * Typical usage:
 * @code
 * StringBuffer buffer;
 * string_buffer_init(&buffer);
 * string_buffer_appendf(&buffer, "int %s;\n", "x");
 * string_buffer_free(&buffer);
 * @endcode
 */
#ifndef STRING_BUFFER_H
#define STRING_BUFFER_H

#include <stddef.h>

/**
 * @brief Dynamically growing, null-terminated string accumulator.
 *
 * @var StringBuffer::data
 * Heap-owned character storage. It is NULL after initialization and may remain
 * NULL until the first append. When non-NULL, it is null-terminated and owned by
 * the buffer.
 *
 * @var StringBuffer::length
 * Number of useful characters currently stored, excluding the trailing null.
 *
 * @var StringBuffer::capacity
 * Allocated byte capacity of @ref data, including room for the trailing null.
 *
 * Valid state: the object must be initialized with string_buffer_init() before
 * use. Invalid state: using an uninitialized object or using it after
 * string_buffer_free() without reinitializing.
 */
typedef struct StringBuffer {
    char *data;
    size_t length;
    size_t capacity;
} StringBuffer;

/**
 * @brief Initialize an empty buffer.
 *
 * @param buffer Borrowed pointer; must not be NULL. The caller owns the
 *        StringBuffer object itself.
 *
 * The function does not allocate immediately. Call before any append operation.
 * No ownership is returned.
 */
void string_buffer_init(StringBuffer *buffer);

/**
 * @brief Free heap storage owned by a buffer and reset it to empty.
 *
 * @param buffer Borrowed pointer; must not be NULL. The buffer may be reused
 *        after calling string_buffer_init() again.
 *
 * The StringBuffer object itself is not freed. No returned ownership.
 */
void string_buffer_free(StringBuffer *buffer);

/**
 * @brief Append formatted text, growing the buffer as needed.
 *
 * @param buffer Borrowed pointer; must not be NULL and must be initialized.
 * @param format Borrowed printf-style format string; must not be NULL.
 * @param ... Format arguments matching @p format.
 *
 * The formatted text is copied into the buffer. The buffer remains
 * null-terminated after the append. On allocation or formatting failure the
 * compiler process exits.
 *
 * No returned ownership.
 */
void string_buffer_appendf(StringBuffer *buffer, const char *format, ...);

/**
 * @brief Append the contents of one buffer into another.
 *
 * @param target Borrowed pointer; must not be NULL and must be initialized.
 * @param source Borrowed pointer; must not be NULL. If source->data is NULL,
 *        nothing is appended.
 *
 * Source text is copied; ownership of @p source and source->data remains with
 * the caller.
 *
 * No returned ownership.
 */
void string_buffer_append_buffer(StringBuffer *target, const StringBuffer *source);

#endif
