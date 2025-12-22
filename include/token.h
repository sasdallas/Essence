/**
 * @file token.h
 * @brief tokens
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _TOKEN_H
#define _TOKEN_H

/**** DEFINITIONS ****/

#define TOKEN_TYPE_EOF                          0
#define TOKEN_TYPE_STRING                       1
#define TOKEN_TYPE_SPACE                        2
#define TOKEN_TYPE_NEWLINE                      3
#define TOKEN_TYPE_SINGLE_QUOTE                 4
#define TOKEN_TYPE_DOUBLE_QUOTE                 5
#define TOKEN_TYPE_REDIRECT_OUT                 6
#define TOKEN_TYPE_REDIRECT_IN                  7
#define TOKEN_TYPE_OR                           8
#define TOKEN_TYPE_PIPE                         9
#define TOKEN_TYPE_AND                          10
#define TOKEN_TYPE_AMPERSAND                    11
#define TOKEN_TYPE_SEMICOLON                    12
#define TOKEN_TYPE_DOLLAR                       13
#define TOKEN_TYPE_STAR                         14
#define TOKEN_TYPE_HASHTAG                      15
#define TOKEN_TYPE_QUESTION_MARK                16
#define TOKEN_TYPE_OPEN_PAREN                   17
#define TOKEN_TYPE_CLOSE_PAREN                  18
#define TOKEN_TYPE_EQUALS                       19
#define TOKEN_TYPE_TILDE                        20

/**** TYPES ****/

typedef struct token {
    int type;                           // Token type
    char *value;                        // Token value
} token_t;

/**** FUNCTIONS ****/

char *token_typeToString(int type);
int token_characterToType(int ch);

#endif