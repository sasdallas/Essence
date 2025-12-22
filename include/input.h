/**
 * @file input.h
 * @brief Input
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _INPUT_H
#define _INPUT_H

/**** INCLUDES ****/
#include <stdio.h>

/**** DEFINITIONS ****/

#define INPUT_TYPE_INTERACTIVE                  0
#define INPUT_TYPE_SCRIPT                       1

#define INPUT_PROMPT_PS1                        0
#define INPUT_PROMPT_PS2                        1

#define INPUT_DEFAULT_BUFFER_SIZE               512

/**** VARIABLES ****/

extern int essence_input_type;
extern int essence_prompt;

extern FILE *input_script;

/**** FUNCTIONS ****/

void input_init();
char *input_get(char *prompt);
int input_getCharacter();
void input_ungetCharacter(int ch);
char *input_getPrompt();
int input_loadScript(char *filename);
int input_loadBuffer(char *buffer);
void input_unloadBuffer();
void input_switchInteractive();

void history_load();
char *history_get(int index);
void history_append(char *str);

#endif