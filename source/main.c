/**
 * @file main.c
 * @brief Main Essence
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
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>

/* Shell argc + argv */
int essence_argc = 1;
char **essence_argv = NULL;

/* PID */
int essence_pid = -1;

void usage() {
    printf("essence, version %d.%d.%d\n", ESSENCE_VERSION_MAJOR, ESSENCE_VERSION_MINOR, ESSENCE_VERSION_LOWER);
    printf("Usage:  essence [OPTION] ...\n");
    printf("        essence [OPTION] script-file ...\n\n");

    printf(" -c COMMAND     Execute command\n");
    printf(" -h, --help     Show this help screen\n");
    printf(" -v, --version  Print out the version and exit\n");
    exit(1);
}

void version() {
    printf("essence version %d.%d.%d\n", ESSENCE_VERSION_MAJOR, ESSENCE_VERSION_MINOR, ESSENCE_VERSION_LOWER);
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(0);
}

/**
 * @brief Source a script
 * @param filename The filename of the script
 * @returns @c cmd_last_exit_status
 */
int essence_runScript(char *filename) {
    // Load script
    if (input_loadScript(filename)) {
        return 127;
    } 

    while (!feof(input_script)) {
        char *input = input_get(NULL);
        if (!input || (*input == EOF)) break;
        parser_interpret();
    }


    // Switchback to interactive
    input_switchInteractive();

    return cmd_last_exit_status;
}


/**
 * @brief Setup shell
 */
void essence_setup() {
// extern void command_setSignals(int i);
    // command_setSignals(0);
    essence_pid = getpid();
    
    // if (setpgid(essence_pid, essence_pid) < 0) {
    //     perror("setpgid");
    //     exit(1);
    // }

    // tcsetpgrp(STDIN_FILENO, essence_pid);
}

/**
 * @brief Main function
 */
int main(int argc, char *argv[]) {
    // Initialize
    essence_setup();

    struct option options[] = {
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
        { 0,0,0,0 }
    };

    // Read arguments
    int ch;
    int index;
    opterr = 1;
    while ((ch = getopt_long(argc, argv, "c:hv", (const struct option*)&options, &index)) != -1) {
        if (ch == 0) ch = options[index].val;
        switch (ch) {
            case 'c':
                input_loadBuffer(optarg);
                parser_interpret(); 
                return cmd_last_exit_status;

            case 'v':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    }


    essence_argc = argc-optind;
    essence_argv = &argv[optind];

    if (argc-optind) {
        return essence_runScript(argv[optind]);
    }

    char buffer[256];
    snprintf(buffer, 256, "%s/.esrc", getenv("HOME"));
    essence_runScript(buffer);

    // Initialize input
    input_init();

    // Now get a prompt and print it out
    while (1) {
        input_get(NULL);
    
        parser_interpret();
    }
    
    return 0;
}