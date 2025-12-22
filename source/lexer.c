/**
 * @file lexer.c
 * @brief Essence shell lexer
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
#include <stdio.h>

/* unget token */
token_t *lexer_unget = NULL;

/**
 * @brief Get a token from the lexer
 * @param prev The previous token from the lexer
 */
token_t *lexer_getToken(token_t *prev) {
    if (lexer_unget) {
        token_t *tok = lexer_unget;
        lexer_unget = NULL;
        return tok;
    }

    int ch = input_getCharacter();
    if (!ch) return NULL;

    token_t *t = malloc(sizeof(token_t));
    t->type = token_characterToType(ch);

    // Are we a string token?
    if (t->type == TOKEN_TYPE_STRING) {
        // We are getting a string token, let's allocate a buffer for it
        char *buffer = malloc(INPUT_DEFAULT_BUFFER_SIZE);
        size_t bufidx = 0;
        size_t bufsz = INPUT_DEFAULT_BUFFER_SIZE;

        buffer[bufidx++] = ch;

        while (1) {
            int ch = input_getCharacter();
            if (token_characterToType(ch) != TOKEN_TYPE_STRING) {
                // Unget this character
                input_ungetCharacter(ch);
                
                // Break out of the string
                buffer[bufidx] = 0;
                t->value = buffer;
                return t;
            }

            // Set character
            buffer[bufidx] = ch;
            bufidx++;

            // Reallocate if needed
            if (bufidx >= bufsz) {
                buffer = realloc(buffer, bufsz*2);
                bufsz *= 2;
            }

            buffer[bufidx] = 0;
        }
    }

    // Handle additional consumption
    if (t->type == TOKEN_TYPE_PIPE && (!prev || prev->type != TOKEN_TYPE_PIPE)) {
        token_t *next = lexer_getToken(t);

        if (next->type == TOKEN_TYPE_PIPE) {
            // Double!
            free(t);
            next->type = TOKEN_TYPE_OR;
            return next;
        }

        // Ah, no, unget this one
        lexer_ungetToken(next);
    }

    if (t->type == TOKEN_TYPE_AMPERSAND && (!prev || prev->type != TOKEN_TYPE_AMPERSAND)) {
        token_t *next = lexer_getToken(t);

        if (next->type == TOKEN_TYPE_AMPERSAND) {
            free(t);
            next->type = TOKEN_TYPE_AND;
            return next;
        }

        lexer_ungetToken(next);
    }

    return t;
}

/**
 * @brief Unget a token from the lexer
 * @param token The token to unget
 */
void lexer_ungetToken(token_t *tok) {
    lexer_unget = tok;
}