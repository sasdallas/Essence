/**
 * @file builtins/exit.c
 * @brief exit command
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

int exit_builtin(int argc, char *argv[]) {
    int status = cmd_last_exit_status;

    if (argc > 1) {
        status = strtol(argv[1], NULL, 10);
    }

    exit(status);
    return status;
}