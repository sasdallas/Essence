/**
 * @file buffer.c
 * @brief Growing buffer library
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "essence.h"
#include <stdlib.h>

/**
 * @brief Create a new buffer
 * @param initial_size Initial size
 */
buffer_t *buffer_create(size_t initial_size) {
    buffer_t *buf = malloc(sizeof(buffer_t));
    buf->buffer = malloc(initial_size);
    buf->bufsz = initial_size;
    buf->bufidx = 0;
    buf->buffer[buf->bufidx] = 0;
    return buf;
}

/**
 * @brief Push character in buffer
 * @param buf The buffer to push the character in
 * @param ch The character to push
 */
void buffer_push(buffer_t *buf, int ch) {
    buf->buffer[buf->bufidx] = ch;
    buf->bufidx++;

    if (buf->bufidx >= buf->bufsz) {
        buf->bufsz *= 2;
        buf->buffer = realloc(buf->buffer, buf->bufsz);
    }

    buf->buffer[buf->bufidx] = 0;
}

/**
 * @brief Push string to buffer
 * @param buf The buffer to push the string in
 * @param str The string to push
 */
void buffer_pushString(buffer_t *buf, char *str) {
    char *p = str;
    while (*p) buffer_push(buf, *p++);
}


/**
 * @brief Pop from buffer
 * @param buf The buffer to pop from
 * @returns 0 on no characters, or character
 */
int buffer_pop(buffer_t *buf) {
    if (!buf->bufidx) return 0;

    // Lower index and get character
    buf->bufidx--;
    int ch = buf->buffer[buf->bufidx];
    buf->buffer[buf->bufidx] = 0;

    return ch;
}

/**
 * @brief Destroy void
 * @param buf The buffer to destroy
 */
void buffer_destroy(buffer_t *buf) {
    free(buf->buffer);
    free(buf);
}