/**
 * @file builtins/builtins.c
 * @brief Builtin list 
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

extern int cd(int argc, char *argv[]);
extern int pwd(int argc, char *argv[]);
extern int exit_builtin(int argc, char *argv[]);
extern int if_cond(int argc, char *argv[]);
extern int then_cond(int argc, char *argv[]);
extern int fi_cond(int argc, char *argv[]);
extern int export(int argc, char *argv[]);


int help(int argc, char *argv[]);

builtin_t builtin_list[] = {
    { .name = "cd", .usage = "cd [dir]", .func = cd },
    { .name = "pwd", .usage = "pwd", .func = pwd },
    { .name = "help", .usage = "help", .func = help },
    { .name = "exit", .usage = "exit [n]", .func = exit_builtin },
    { .name = "export", .usage = "export [var]=[value]", .func = export}
};

const int builtin_list_size = sizeof(builtin_list) / sizeof(builtin_t);

int help(int argc, char *argv[]) {
    printf("Essence v%d.%d.%d\n\n", ESSENCE_VERSION_MAJOR, ESSENCE_VERSION_MINOR, ESSENCE_VERSION_LOWER);
    printf("Available commands:\n");

    for (int i = 0; i < sizeof(builtin_list) / sizeof(builtin_t); i++) {
        printf(" %s\n", builtin_list[i].usage);
    }

    return 0;
}