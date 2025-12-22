/**
 * @file builtins/pwd.c
 * @brief pwd command
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
#include <unistd.h>
#include <stdio.h>

int pwd(int argc, char *argv[]) {
    char cwd[256];
    if (getcwd(cwd, 256) < 0) {
        perror("getcwd");
        return 1;
    }

    printf("%s\n", cwd);
    return 0;
}