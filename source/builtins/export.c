/**
 * @file source/builtins/export.c
 * @brief export 
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;

int export(int argc, char *argv[]) {
    if (argc == 1) {
        char **e = environ;
        while (*e) {
            printf("%s\n", *e);
            e++;
        }
    } else {
        for (int i = 1; i < argc; i++) {
            putenv(strdup(argv[i])); // Stupid putenv... the strdup is required.
        }
    }

    return 0;
}