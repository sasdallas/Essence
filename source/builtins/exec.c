/**
 * @file builtins/exec.c
 * @brief exec builtin
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
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int exec_builtin(int argc, char *argv[]) {
    if (argc <= 1) {
        return 0;
    }

    execvp(argv[1], &argv[1]);
    fprintf(stderr, "essence: exec: %s\n", strerror(errno));
    return 1;
}