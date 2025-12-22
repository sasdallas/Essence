/**
 * @file parser.h
 * @brief Parser
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _PARSER_H
#define _PARSER_H

/**** INCLUDES ****/
#include "token.h"

/**** FUNCTIONS ****/

void parser_interpret();
void parser_syntaxError(token_t *tok);

#endif