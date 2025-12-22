/**
 * @file builtins/cd.c
 * @brief cd command
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
#include <errno.h>

int cd(int argc, char *argv[]) {
    char *dir = getenv("HOME");
    if (argc > 1) dir = argv[1];
    if (!dir) return 0;

    if (chdir(argv[1]) < 0) {
        fprintf(stderr, "essence: %s: %s\n", dir, strerror(errno));
        return 1;
    }

    return 0;
}