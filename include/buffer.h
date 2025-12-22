/**
 * @file buffer.h
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

#ifndef _BUFFER_H
#define _BUFFER_H

/**** INCLUDES ****/
#include <stddef.h>

/**** TYPES ****/

typedef struct buffer {
    char *buffer;
    size_t bufidx;
    size_t bufsz;
} buffer_t;

/**** FUNCTIONS ****/

buffer_t *buffer_create(size_t initial_size);
void buffer_push(buffer_t *buf, int ch);
int buffer_pop(buffer_t *buf);
void buffer_destroy(buffer_t *buf);
void buffer_pushString(buffer_t *buf, char *str);

#endif