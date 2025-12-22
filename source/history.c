/**
 * @file history.c
 * @brief History
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* History buffer */
char **history_buffer = NULL;
size_t history_buffer_size = 0;
size_t history_buffer_len = 0;

/* History file */
FILE *history_file = NULL;

/**
 * @brief Flush history file
 */
void history_flush() {

}

/**
 * @brief Load history file
 */
void history_load() {
    atexit(history_flush);

    // Open history file
    char history_path[256];
    snprintf(history_path, 256, "%s/.history", getenv("HOME"));
    history_file = fopen(history_path, "r");

    // Create buffer
    history_buffer = malloc(12 * sizeof(char*));
    history_buffer_size = 12;
    history_buffer_len = 0;
}

/**
 * @brief Get a history entry
 * @param index The index
 */
char *history_get(int index) {
    if (!history_buffer) return NULL;
    
    if (index >= history_buffer_len) {
        // We must pull from the file
        if (history_file) {
            return NULL;
        }

        return NULL;
    }

    return history_buffer[history_buffer_len-(index)-1];
}

/**
 * @brief Append a history entry
 */
void history_append(char *str) {
    if (!history_buffer) return;
    
    // Temporarily remove newline
    char *nl = (strchr(str, '\n'));
    if (nl) *nl = 0;

    // Check, was the previous entry the same?
    if (history_buffer_len && !strcmp(history_buffer[history_buffer_len-1], str)) {
        // Yes, don't bother
        if (nl) *nl = '\n';
        return;
    }
    
    history_buffer[history_buffer_len] = strdup(str);
    if (nl) *nl = '\n';

    history_buffer_len++;

    if (history_buffer_len >= history_buffer_size) {
        history_buffer = realloc(history_buffer, sizeof(char*) * (history_buffer_size*2));
        history_buffer_size *= 2;
    }
}