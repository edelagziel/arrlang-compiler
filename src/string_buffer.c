/**
 * @file string_buffer.c
 * @brief Implementation of growable text buffers for generated C fragments.
 *
 * Responsibilities: initialize buffers, append formatted text, append copied
 * buffer contents, grow heap storage, and release owned memory. This module
 * does not interpret the text it stores or know about ArrLang syntax.
 *
 * Main dependencies: stdarg/stdiod for formatting and stdlib for allocation.
 * Typical flow: initialize, append many fragments, read buffer->data, free.
 */
#include "string_buffer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void string_buffer_init(StringBuffer *buffer)
{
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

void string_buffer_free(StringBuffer *buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

void string_buffer_appendf(StringBuffer *buffer, const char *format, ...)
{
    va_list args;
    va_list args_copy;
    int required;
    size_t available;
    char *grown_data;

    va_start(args, format);
    va_copy(args_copy, args);
    required = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (required < 0) {
        fprintf(stderr, "Internal error while formatting generated code.\n");
        exit(1);
    }

    /* Grow geometrically so repeated code-generation appends remain cheap. */
    available = buffer->capacity > buffer->length ? buffer->capacity - buffer->length : 0;
    if ((size_t)required + 1 > available) {
        size_t new_capacity = buffer->capacity == 0 ? 128 : buffer->capacity;

        while ((size_t)required + 1 > new_capacity - buffer->length) {
            new_capacity *= 2;
        }

        grown_data = realloc(buffer->data, new_capacity);
        if (grown_data == NULL) {
            fprintf(stderr, "Out of memory while building generated code.\n");
            exit(1);
        }

        buffer->data = grown_data;
        buffer->capacity = new_capacity;
    }

    vsnprintf(buffer->data + buffer->length, buffer->capacity - buffer->length, format, args);
    va_end(args);
    buffer->length += (size_t)required;
}

void string_buffer_append_buffer(StringBuffer *target, const StringBuffer *source)
{
    if (source->data != NULL) {
        string_buffer_appendf(target, "%s", source->data);
    }
}
