/**
 * @file lexer.h
 * @brief Essence lexer
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _LEXER_H
#define _LEXER_H

/**** INCLUDES ****/
#include "token.h"

/**** FUNCTIONS ****/

token_t *lexer_getToken(token_t *prev);
void lexer_ungetToken(token_t *tok);

#endif