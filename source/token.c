/**
 * @file token.c
 * @brief Token functions
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
#include <stdio.h>

/**
 * @brief Convert a single character into a type
 * @param ch The character to convert
 */
int token_characterToType(int ch) {
    switch (ch) {
        case EOF:
            return TOKEN_TYPE_EOF;
        case '\n':
            return TOKEN_TYPE_NEWLINE;
        case ' ':
            return TOKEN_TYPE_SPACE;
        case '\'':
            return TOKEN_TYPE_SINGLE_QUOTE;
        case '\"':
            return TOKEN_TYPE_DOUBLE_QUOTE;
        case '>':
            return TOKEN_TYPE_REDIRECT_OUT;
        case '<':
            return TOKEN_TYPE_REDIRECT_IN;
        case '|':
            return TOKEN_TYPE_PIPE;
        case '&':
            return TOKEN_TYPE_AMPERSAND;
        case ';':
            return TOKEN_TYPE_SEMICOLON;
        case '$':
            return TOKEN_TYPE_DOLLAR;
        case '*':
            return TOKEN_TYPE_STAR;
        case '#':
            return TOKEN_TYPE_HASHTAG;
        case '?':
            return TOKEN_TYPE_QUESTION_MARK;
        case '(':
            return TOKEN_TYPE_OPEN_PAREN;
        case ')':
            return TOKEN_TYPE_CLOSE_PAREN;
        case '=':
            return TOKEN_TYPE_EQUALS;
        case '~':
            return TOKEN_TYPE_TILDE;
        default:
            return TOKEN_TYPE_STRING;
    }
}

/**
 * @brief Get token type as string
 * @param type The type
 */
char *token_typeToString(int type) {
    switch (type) {
        case TOKEN_TYPE_EOF:
            return "<eof>";
        case TOKEN_TYPE_NEWLINE:
            return "<newline>";
        case TOKEN_TYPE_SPACE:
            return "<space>";
        case TOKEN_TYPE_STRING:
            return "<string>";
        case TOKEN_TYPE_AND:
            return "<and>";
        case TOKEN_TYPE_OR:
            return "<or>";
        case TOKEN_TYPE_OPEN_PAREN:
        case TOKEN_TYPE_CLOSE_PAREN:    
            return "<paren>";
        default:
            return "<unknown>";
    }
}