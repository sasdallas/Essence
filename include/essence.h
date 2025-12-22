/**
 * @file essence.h
 * @brief Essence header file
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _ESSENCE_H
#define _ESSENCE_H

/**** INCLUDES ****/
#include "token.h"
#include "input.h"
#include "lexer.h"
#include "parser.h"
#include "command.h"
#include "buffer.h"

/**** DEFINITIONS ****/

#define ESSENCE_VERSION_MAJOR       2
#define ESSENCE_VERSION_MINOR       0
#define ESSENCE_VERSION_LOWER       0

/**** VARIABLES ****/

extern int essence_argc;
extern char **essence_argv;
extern int essence_pid;

#endif