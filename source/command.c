/**
 * @file command.c
 * @brief Command handler
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
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>



/* Last command exit status */
int cmd_last_exit_status = 0;
int cmd_waitpid_exit_status = 0;
int cmd_last_signalled = 0;

/**
 * @brief Set signals
 */
void command_setSignals(int o) {
    signal(SIGINT, o ? SIG_DFL : SIG_IGN);
    signal(SIGQUIT, o ? SIG_DFL : SIG_IGN);
    signal(SIGTSTP, o ? SIG_DFL : SIG_IGN);
    signal(SIGTTIN, o ? SIG_DFL : SIG_IGN);
    signal(SIGTTOU, o ? SIG_DFL : SIG_IGN);
}

/**
 * @brief Execute a single command
 * @param command The command to execute
 * @returns Exit status
 */
int command_execute(command_t *command) {
    if (!command->argc) {
        // If any environ were specified, we might have to put them
        if (command->additional_envp) {
            char **env = command->additional_envp;

            while (*env) {
                putenv(strdup(*env));
                env++;
            }
        }
        
        return cmd_last_exit_status;
    }

    // Check builtin
    for (int i = 0; i < builtin_list_size; i++) {
        if (!strcmp(builtin_list[i].name, command->argv[0])) {
            // Match! Execute this!
            cmd_last_exit_status = builtin_list[i].func(command->argc, command->argv);
            return cmd_last_exit_status;
        }
    }

    // Execute the command
    pid_t cpid = fork();

    if (!cpid) { 
        // Enable signals
        command_setSignals(1);

        // We are the child, setup our data
        if (command->additional_envp) {
            char **env = command->additional_envp;

            while (*env) {
                putenv(*env);
                env++;
            }
        }

        // Duplicate file descriptors if we need it
        if (command->stdin) dup2(command->stdin, STDIN_FILENO);
        if (command->stdout) dup2(command->stdout, STDOUT_FILENO);
        if (command->stderr) dup2(command->stderr, STDERR_FILENO);

        // Execute the command!
        execvp(command->argv[0], command->argv);
        
        if (errno == ENOENT) {
            fprintf(stderr, "essence: %s: command not found\n", command->argv[0]);
            command_cleanup(command);
            _exit(127);
        }

        // Error
        fprintf(stderr, "essence: %s: %s\n", command->argv[0], strerror(errno));
        command_cleanup(command);
        _exit(126);
        __builtin_unreachable();
    }

    // set to child
    setpgid(cpid, cpid);
    tcsetpgrp(STDIN_FILENO, cpid);

    // Wait on the child
    // TODO: Job control
    int wstatus;
    int w = -1;
    do {
	    w = waitpid(cpid, &wstatus, 0);
    } while (w == -1 && errno == EINTR);

    if (w < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    // restore
    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(STDIN_FILENO, getpid());
    signal(SIGTTOU, SIG_DFL);

    cmd_last_signalled = WIFSIGNALED(wstatus);
    if (cmd_last_signalled) {
        fprintf(stderr, "essence: Process \"%s\" terminated by signal %s\n", command->argv[0], strsignal(WTERMSIG(wstatus)));
    }

    cmd_last_exit_status = WEXITSTATUS(wstatus);
    cmd_waitpid_exit_status = wstatus;
    return WEXITSTATUS(wstatus);
}

/**
 * @brief Execute a list of commands
 * @param command List
 * @param command_count Command count
 */
void command_executeList(command_t *command, size_t command_count) {
    for (unsigned i = 0; i < command_count; ) {
        if (i && (command[i].exec_flags & COMMAND_FLAG_OR)) {
            if (!cmd_last_exit_status) { i++; continue; }
        }
        if (i && (command[i].exec_flags & COMMAND_FLAG_AND)) {
            if (cmd_last_exit_status) { i++; continue; }
        }

        unsigned start = i;
        unsigned end = i;
        while (end + 1 < command_count && (command[end + 1].exec_flags & COMMAND_FLAG_PIPE_FROM_PREV)) {
            end++;
        }

        if (start == end) {
            int status = command_execute(&command[start]);
            if (!(command[start].exec_flags & COMMAND_FLAG_JOB)) cmd_last_exit_status = status;
            i = end + 1;
            continue;
        }

        size_t n = end - start + 1;
        int pipes[ (n - 1) * 2 ];
        for (size_t p = 0; p < n - 1; p++) {
            if (pipe(&pipes[p*2]) < 0) {
                perror("pipe");
                return;
            }
        }

        pid_t pids[n];
        memset(pids, 0, sizeof(pids));

        for (size_t idx = 0; idx < n; idx++) {
            command_t *cmd = &command[start + idx];
            command_execute(cmd);

            if (WIFSIGNALED(cmd_waitpid_exit_status)) {
                break;
            }
        }

        for (size_t p = 0; p < n - 1; p++) {
            close(pipes[p*2]);
            close(pipes[p*2 + 1]);
        }

        i = end + 1;
    }
}

/**
 * @brief Cleanup and free command resources
 * @param command The command to cleanup
 */
void command_cleanup(command_t *command) {
    if (command->additional_envp) {
        char **envp = command->additional_envp;
        while (*envp) {
            free(*envp);
            envp++;
        } 

        free(command->additional_envp);
    }

    for (int i = 0; i < command->argc; i++) {
        free(command->argv[i]);
    }

    free(command->argv);

    // Close file descriptors
    if (command->stdin != -1) close(command->stdin);
    if (command->stdout != -1) close(command->stdout);
    if (command->stderr != -1) close(command->stderr);
}
