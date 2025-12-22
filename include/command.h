/**
 * @file command.h
 * @brief Command logic
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _COMMAND_H
#define _COMMAND_H

/**** INCLUDES ****/

#include <stdlib.h>
#include <string.h>

/**** DEFINITIONS ****/

#define COMMAND_FLAG_OR             0x01    // Execute if the command behind it had an exit status != 1
#define COMMAND_FLAG_AND            0x02    // Execute if the command behind it had an exit status of 0
#define COMMAND_FLAG_JOB            0x04    // This command is a job
#define COMMAND_FLAG_PIPE_FROM_PREV 0x08    // This command's stdin comes from previous via pipe

/**** TYPES ****/

typedef struct command {
    int argc;                   // Argument count
    char **argv;                // Argument list
    int envc;                   // Additional environ count
    char **additional_envp;     // Additional environs

    int exec_flags;             // Execution flags

    int stdin;                  // Redirect stdin to this fd (-1 = no redir)
    int stdout;                 // Redirect stdout to this fd (-1 = no redir)
    int stderr;                 // Redirect stderr to this fd (-1 = no redir)
} command_t;

typedef int (*builtin_func_t)(int argc, char **argv);

typedef struct builtin {
    char *name;                 // Builtin name
    builtin_func_t func;        // Function
    char *usage;                // Usage
} builtin_t;

/**** VARIABLES ****/

extern int cmd_last_exit_status;
extern int cmd_last_signalled;

extern builtin_t builtin_list[];
extern const int builtin_list_size;

/**** MACROS ****/

#define COMMAND_INIT(cmd) ({  (cmd)->argc = 0; (cmd)->additional_envp = NULL; (cmd)->stdin = -1; (cmd)->stdout = -1; (cmd)->stderr = -1; (cmd)->argv = malloc(sizeof(char*)); (cmd)->argv[0] = NULL; (cmd)->exec_flags = 0x0; (cmd)->envc = 0; })
#define COMMAND_LIST_INIT() ({ command_t *cmd = malloc(sizeof(command_t)); COMMAND_INIT(cmd); cmd; })
#define COMMAND_PUSH_ARGV(cmd, arg) ({ (cmd)->argc++; (cmd)->argv = realloc((cmd)->argv, ((cmd)->argc+1) * sizeof(char*)); (cmd)->argv[(cmd)->argc-1] = arg; (cmd)->argv[(cmd)->argc] = NULL; })
#define COMMAND_PUSH_ENVIRON(cmd, env) ({ (cmd)->envc++; if (!(cmd)->additional_envp) { (cmd)->additional_envp = malloc(sizeof(char*) * ((cmd)->envc+1)); (cmd)->additional_envp[1] = NULL; } else { (cmd)->additional_envp = realloc((cmd)->additional_envp, sizeof(char*) * ((cmd)->envc + 1)); }; (cmd)->additional_envp[(cmd)->envc-1] = env; (cmd)->additional_envp[(cmd)->envc] = NULL; })

#define COMMAND_NEW(list, new_count) ({ list = realloc(list, new_count * sizeof(command_t)); COMMAND_INIT((&list[new_count-1])); })

/**** FUNCTIONS ****/

int command_execute(command_t *command);
void command_executeList(command_t *command, size_t command_count);
void command_cleanup(command_t *command);

#endif