/**
 * @file parser.c
 * @brief Token parser
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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>

/* Current parser state */
int parser_quoted = 0;              // Parser has encountered double or single quotes
int parser_single_quoted = 0;       // Parser has encountered single quotes
int parser_pending_redirect = 0;    // Parser is now pending a redirection on stdin
int parser_pending_fd = 0;          // Parser pending redirection fd

/* Current command being processed */
#define CMD                 (cmds[cmd_count-1])

/* Buffer macros */
#define BUFFER_GROW() ({ bufsz *= 2; buf = realloc(buf, bufsz); })
#define BUFFER_PUSH(ch) ({ buf[bufidx] = ch; bufidx++; if (bufidx >= bufsz) { BUFFER_GROW(); }; buf[bufidx] = 0; })
#define BUFFER_POP() ({ char ch = 0; if (bufidx) { ch = buf[bufidx-1]; bufidx--;} }; ch; )

/* Next token */
#define NEXT_TOKEN() goto _next_token

/* Token quoted case */
#define TOKEN_IGNORE_QUOTED(ch) if (parser_quoted) { BUFFER_PUSH(ch); NEXT_TOKEN(); }

/**
 * @brief Syntax error in parser
 * @param tok The erroring token
 */
void parser_syntaxError(token_t *tok) {
    fprintf(stderr, "essence: syntax error near unexpected token %s\n", token_typeToString(tok->type));
}

/**
 * @brief Parse variable from token
 * @param tok The current token being parsed
 */
char *parser_interpretVariable(token_t *tok) {
    // Dollar sign indicates that we need an environ. Get the next token.
    token_t *next = lexer_getToken(tok);

    // Depending on what next->type is..
    char tmp[128];
    if (next->type == TOKEN_TYPE_DOLLAR) {
        snprintf(tmp, 128, "%d", essence_pid);
    } else if (next->type == TOKEN_TYPE_HASHTAG) {
        snprintf(tmp, 128, "%d", essence_argc);
    } else if (next->type == TOKEN_TYPE_QUESTION_MARK) {
        snprintf(tmp, 128, "%d", cmd_last_exit_status);
    } else if (next->type == TOKEN_TYPE_STRING) {
        char *nl = strchr(next->value, '\n');
        if (nl) *nl = 0;

        // Just a variable
        if (!strcmp(next->value, "RANDOM")) {
            snprintf(tmp, 128, "%d", rand() % RAND_MAX);
        } else {
            char *env = getenv(next->value);

            if (env) snprintf(tmp, 128, "%s", env);
            else tmp[0] = 0;
        }
        
        free(next->value);
    } else if (next->type == TOKEN_TYPE_OPEN_PAREN) {
        // This is more complex, we need to run the command inside of this.
        // Construct the command 
        char *cmd = malloc(128);
        size_t cmdsize = 128;
        size_t cmdidx = 0;

        while (1) {
            int ch = input_getCharacter();
            if (ch == EOF) {
                fprintf(stderr, "essence: unexpected EOF when looking for matching \')\'\n");
                return NULL;
            }

            if (ch == '\n') {
                // We haven't found a new line..
                // Try to get another line of input
                essence_prompt = INPUT_PROMPT_PS2;
                input_get(NULL);
                essence_prompt = INPUT_PROMPT_PS1;
                continue;
            }


            if (ch == ')') {
                // We're done here
                break;
            }

            // Push
            cmd[cmdidx] = ch;
            cmdidx++;

            // Grow
            if (cmdidx >= cmdsize) {
                cmdsize *= 2;
                cmd = realloc(cmd, cmdsize);
            }

            cmd[cmdidx] = 0;
        }

        // Make a new pipe
        int pfd[2];
        if (pipe(pfd) < 0) {
            perror("pipe");
            exit(1);
        }

        // Fork, launch a shell subprocess
        pid_t cpid = fork();
        if (!cpid) {
            dup2(pfd[1], STDOUT_FILENO);
            
            // write one character to stdout
            write(STDOUT_FILENO, "a", 1);

            char *nargv[4] = { "essence", "-c", cmd, NULL };
            execvp("essence", nargv);
            exit(127);
        }

        // Wait, read from pipe. 
        // TODO: WHAT IF IT HAS MORE OUTPUT, THIS IS SILLY

        setpgid(cpid, cpid);
        tcsetpgrp(STDIN_FILENO, cpid);
        waitpid(cpid, NULL, 0);

        signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
        signal(SIGTTOU, SIG_DFL);

        ssize_t r = read(pfd[0], tmp, 128);
        free(cmd);

        if (r == 1) {
            free(next);
            return strdup("");
        }

        char *p = &tmp[1];
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
    } else {
        // Normal character
        snprintf(tmp, 128, "$");
    }

    free(next);
    return strdup(&tmp[1]);
}

/**
 * @brief Finalize redirection
 */
static int parser_finalizeRedir(command_t *cmd, char *buf, size_t *pbufidx) {
    int f = open(buf, (parser_pending_fd == STDIN_FILENO) ? O_RDONLY : O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
    if (f < 0) {
        fprintf(stderr, "essence: %s: %s\n", buf, strerror(errno));
        return -1;
    }

    switch (parser_pending_fd) {
        case STDIN_FILENO: cmd->stdin = f; break;
        case STDOUT_FILENO: cmd->stdout = f; break;
        case STDERR_FILENO: cmd->stderr = f; break;
    }

    parser_pending_redirect = 0;
    *pbufidx = 0;
    return 0;
}


/**
 * @brief Parse until found
 */
static int parser_parseUntil(const char *stop1, const char *stop2, command_t **out_list, int *out_count) {
    token_t *tok = NULL;

    command_t *cmds = COMMAND_LIST_INIT();
    int cmd_count = 1;

    char *buf = malloc(512);
    size_t bufidx = 0;
    size_t bufsz = 512;

    parser_quoted = 0;
    parser_single_quoted = 0;
    parser_pending_redirect = 0;
    parser_pending_fd = 0;

    while (1) {
        token_t *new = lexer_getToken(tok);
        if (tok) free(tok);
        tok = new;
        if (!new) break;

        if (new->type == TOKEN_TYPE_STRING && !parser_quoted && !parser_single_quoted && !parser_pending_redirect && bufidx == 0 && CMD.argc == 0) {
            if (stop1 && !strcmp(new->value, stop1)) {
                free(new->value);
                free(new);
                tok = NULL;
                break;
            }
            if (stop2 && !strcmp(new->value, stop2)) {
                free(new->value);
                free(new);
                tok = NULL;
                *out_list = cmds;
                while (cmd_count > 0 && cmds[cmd_count-1].argc == 0) cmd_count--;
                *out_count = -cmd_count;
                free(buf);
                return 0;
            }
        }

        switch (new->type) {
            case TOKEN_TYPE_SPACE:
                if (!bufidx) NEXT_TOKEN();

                if (parser_quoted) { BUFFER_PUSH(' '); NEXT_TOKEN(); }

                if (parser_pending_redirect) {
                    if (parser_finalizeRedir(&CMD, buf, &bufidx) < 0) {
                        /* error: drop last command */
                        cmd_count--; goto _done_list;
                    }
                    NEXT_TOKEN();
                }

                COMMAND_PUSH_ARGV((&CMD), strdup(buf));
                bufidx = 0;
                NEXT_TOKEN();

            case TOKEN_TYPE_STRING:
                if (bufidx + strlen(new->value) >= bufsz) BUFFER_GROW();
                strncpy(buf + bufidx, new->value, bufsz - bufidx);
                bufidx += strlen(new->value);
                free(new->value);
                NEXT_TOKEN();

            case TOKEN_TYPE_DOUBLE_QUOTE:
                parser_quoted = !parser_quoted; break;

            case TOKEN_TYPE_SINGLE_QUOTE:
                if (parser_quoted && !parser_single_quoted) { BUFFER_PUSH('\''); NEXT_TOKEN(); }
                parser_single_quoted = !parser_single_quoted; parser_quoted = !parser_quoted; NEXT_TOKEN();

            case TOKEN_TYPE_REDIRECT_OUT:
                if (parser_quoted) { BUFFER_PUSH('>'); NEXT_TOKEN(); }
                parser_pending_fd = STDOUT_FILENO; parser_pending_redirect = 1;
                /* consume spaces */
                { token_t *spc = lexer_getToken(new); while (spc && spc->type == TOKEN_TYPE_SPACE) { token_t *nx = lexer_getToken(spc); free(spc); spc = nx; } lexer_ungetToken(spc); }
                NEXT_TOKEN();

            case TOKEN_TYPE_OR:
                if (parser_quoted) { BUFFER_PUSH('|'); BUFFER_PUSH('|'); NEXT_TOKEN(); }
                if (!CMD.argc || parser_pending_redirect) { parser_syntaxError(new); goto _done_list; }
                if (bufidx) { COMMAND_PUSH_ARGV((&CMD), strdup(buf)); bufidx = 0; }
                COMMAND_NEW(cmds, (cmd_count+1)); cmd_count += 1; CMD.exec_flags |= COMMAND_FLAG_OR; NEXT_TOKEN();

            case TOKEN_TYPE_AND:
                if (parser_quoted) { BUFFER_PUSH('&'); BUFFER_PUSH('&'); NEXT_TOKEN(); }
                if (!CMD.argc || parser_pending_redirect) { parser_syntaxError(new); goto _done_list; }
                if (bufidx) { COMMAND_PUSH_ARGV((&CMD), strdup(buf)); bufidx = 0; }
                COMMAND_NEW(cmds, (cmd_count+1)); cmd_count += 1; CMD.exec_flags |= COMMAND_FLAG_AND; NEXT_TOKEN();

            case TOKEN_TYPE_SEMICOLON:
                TOKEN_IGNORE_QUOTED(';');
                if (!CMD.argc && !bufidx) { parser_syntaxError(new); goto _done_list; }
                if (bufidx) { COMMAND_PUSH_ARGV((&CMD), strdup(buf)); bufidx = 0; }
                COMMAND_NEW(cmds, (cmd_count+1)); cmd_count += 1; NEXT_TOKEN();

            case TOKEN_TYPE_EQUALS: {
                TOKEN_IGNORE_QUOTED('=');
                if (CMD.argc || !bufidx) { BUFFER_PUSH('='); NEXT_TOKEN(); }

                token_t *nxt = lexer_getToken(new);
                buffer_t *env_buffer = buffer_create(128);
                while (nxt && nxt->type != TOKEN_TYPE_EOF && nxt->type != TOKEN_TYPE_NEWLINE) {
                    if (nxt->type == TOKEN_TYPE_SPACE && !parser_quoted) { break; }
                    else if (nxt->type == TOKEN_TYPE_DOUBLE_QUOTE) { if (parser_single_quoted) { buffer_push(env_buffer, '"'); goto _equals_next_token_if; } parser_quoted = !parser_quoted; }
                    else if (nxt->type == TOKEN_TYPE_SINGLE_QUOTE) { if (parser_quoted && !parser_single_quoted) { buffer_push(env_buffer, '\''); goto _equals_next_token_if; } parser_quoted = !parser_quoted; parser_single_quoted = !parser_single_quoted; }
                    else if (nxt->type == TOKEN_TYPE_STRING) { buffer_pushString(env_buffer, nxt->value); free(nxt->value); }
                    else if (nxt->type == TOKEN_TYPE_DOLLAR) { if (parser_single_quoted) { buffer_push(env_buffer, '$'); goto _equals_next_token_if; } char *var = parser_interpretVariable(nxt); if (var) { buffer_pushString(env_buffer, var); free(var); } }
                _equals_next_token_if:
                    { token_t *nxt2 = lexer_getToken(nxt); free(nxt); nxt = nxt2; }
                }
                if (nxt) lexer_ungetToken(nxt);

                char *environ_statement = malloc(strlen(env_buffer->buffer) + 1 + strlen(buf) + 1);
                sprintf(environ_statement, "%s=%s", buf, env_buffer->buffer);
                buffer_destroy(env_buffer);
                COMMAND_PUSH_ENVIRON(&CMD, environ_statement);
                bufidx = 0;
                NEXT_TOKEN();
            }

            case TOKEN_TYPE_NEWLINE:
            case TOKEN_TYPE_EOF:
                if (parser_pending_redirect) {
                    if (bufidx) {
                        if (parser_finalizeRedir(&CMD, buf, &bufidx) < 0) { cmd_count--; goto _done_list; }
                        break;
                    }
                    parser_syntaxError(new);
                    goto _done_list;
                }
                if (bufidx) { COMMAND_PUSH_ARGV((&CMD), strdup(buf)); bufidx = 0; }
                
                COMMAND_NEW(cmds, (cmd_count+1)); cmd_count += 1;

                essence_prompt = INPUT_PROMPT_PS2;
                input_get(NULL);
                essence_prompt = INPUT_PROMPT_PS1;
                break;

            case TOKEN_TYPE_DOLLAR: {
                if (parser_single_quoted) { BUFFER_PUSH('$'); NEXT_TOKEN(); }
                char *tmp = parser_interpretVariable(new);
                if (tmp) { char *p = tmp; while (*p) { BUFFER_PUSH(*p); p++; } free(tmp); }
                NEXT_TOKEN();
            }

            case TOKEN_TYPE_TILDE: {
                TOKEN_IGNORE_QUOTED('~');
                char *home = getenv("HOME"); if (!home) home = "/root/";
                char *p = home; while (*p) { BUFFER_PUSH(*p); p++; }
                NEXT_TOKEN();
            }

            case TOKEN_TYPE_HASHTAG: {
                TOKEN_IGNORE_QUOTED('#');
                token_t *n = lexer_getToken(new);
                while (n && n->type != TOKEN_TYPE_NEWLINE && n->type != TOKEN_TYPE_EOF) {
                    if (n->type == TOKEN_TYPE_STRING) free(n->value);
                    token_t *n2 = lexer_getToken(n); free(n); n = n2;
                }
                lexer_ungetToken(n); break;
            }

            default:
                fprintf(stderr, "essence: parser: Unrecognized token %d\n", new->type);
                NEXT_TOKEN();
        }

    _next_token: ;
    }

_done_list:
    if (tok) free(tok);

    while (cmd_count > 0 && cmds[cmd_count-1].argc == 0) cmd_count--;

    *out_list = cmds;
    *out_count = cmd_count;

    free(buf);
    return 0;
}

/**
 * @brief Parse an if statement
 */
static void parser_if() {
    command_t *cond_list = NULL;
    int cond_count = 0;
    if (parser_parseUntil("then", NULL, &cond_list, &cond_count) != 0) return;

    command_t *then_list = NULL;
    int then_count = 0;
    if (parser_parseUntil("else", "fi", &then_list, &then_count) != 0) {
        for (int i = 0; i < cond_count; i++) command_cleanup(&cond_list[i]);
        free(cond_list);
        return;
    }

    int matched_is_fi = 0;
    if (then_count < 0) {
        then_count = -then_count;
        matched_is_fi = 1;
    }

    command_t *else_list = NULL;
    int else_count = 0;
    if (!matched_is_fi) {
        if (parser_parseUntil("fi", NULL, &else_list, &else_count) != 0) {
            for (int i = 0; i < cond_count; i++) command_cleanup(&cond_list[i]);
            for (int i = 0; i < then_count; i++) command_cleanup(&then_list[i]);
            free(cond_list);
            free(then_list);
            return;
        }
    }

    command_executeList(cond_list, cond_count);

    if (!cmd_last_exit_status) {
        if (then_count > 0) command_executeList(then_list, then_count);
    } else {
        if (else_list && else_count > 0) command_executeList(else_list, else_count);
    }

    for (int i = 0; i < cond_count; i++) command_cleanup(&cond_list[i]);
    for (int i = 0; i < then_count; i++) command_cleanup(&then_list[i]);
    if (else_list) {
        for (int i = 0; i < else_count; i++) command_cleanup(&else_list[i]);
    }
    
    free(cond_list); 
    free(then_list); 
    if (else_list) free(else_list);
}

/**
 * @brief while parser
 */
static void parser_while() {
    command_t *cond_list = NULL;
    int cond_count = 0;
    if (parser_parseUntil("do", NULL, &cond_list, &cond_count) != 0) return;

    command_t *cmd_list = NULL; 
    int cmd_count = 0;
    if (parser_parseUntil("done", NULL, &cmd_list, &cmd_count) != 0) {
        // Cleanup memory
        for (int i = 0; i < cond_count; i++) command_cleanup(&cond_list[i]);
        free(cond_list);
    }

    while (1) {
        command_executeList(cond_list, cond_count);
        if (cmd_last_signalled) break;
    

        if (!cmd_last_exit_status) {
            command_executeList(cmd_list, cmd_count);
            if (cmd_last_signalled) break;
        } else {
            break;
        }
    }

    // cleanup resources
    
    for (int i = 0; i < cond_count; i++) command_cleanup(&cond_list[i]);
    free(cond_list);

    for (int i = 0; i < cmd_count; i++) command_cleanup(&cmd_list[i]);
    free(cmd_list);
}

/**
 * @brief Check and process special token
 */
int parser_checkToken(token_t *tok) {
    if (!strcmp(tok->value, "if")) {
        parser_if();
        return 1;
    } else if (!strcmp(tok->value, "while")) {
        parser_while();
        return 1;
    } else if (!strcmp(tok->value, "else")) {
        return -1;
    }

    return 0;
}

/**
 * @brief Main interpret function
 */
void parser_interpret() {
    token_t *tok = NULL;

    // One command
    command_t *cmds = COMMAND_LIST_INIT();
    int cmd_count = 1;

    // This argument buffer, currently
    char *buf = malloc(512);
    size_t bufidx = 0;
    size_t bufsz = 512;

    // Enter parser loop
    while (1) {
        // Get a token
        token_t *new = lexer_getToken(tok);
        if (tok) free(tok);
        tok = new;
        if (!new) break;

        // Now start processing this token
        switch (tok->type) {
            case TOKEN_TYPE_SPACE:
                if (!bufidx) NEXT_TOKEN();

                // Space, go to next argument
                if (parser_quoted) {
                    // We are quoted, instead just push
                    BUFFER_PUSH(' ');
                    NEXT_TOKEN();
                }
                
                // Are we pending a redirection?
                if (parser_pending_redirect) {
                    parser_pending_redirect = 0;

                    // Yes, let's redirect!
                    // The data should be contained in buffer since we haven't pushed a new argv
                    // Open the file

                    int f = open(buf, (parser_pending_fd == STDIN_FILENO) ? O_RDONLY : O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
                    if (f < 0) {
                        fprintf(stderr, "essence: %s: %s\n", buf, strerror(errno));
                        
                        // TODO: Remove this command *properly*
                        cmd_count--;

                        goto _execute;
                    }

                    switch (parser_pending_fd) {
                        case STDIN_FILENO:
                            CMD.stdin = f;
                            break;
                        case STDOUT_FILENO:
                            CMD.stdout = f;
                            break;
                        case STDERR_FILENO:
                            CMD.stderr = f;
                            break;
                    }

                    bufidx = 0;
                    NEXT_TOKEN();
                }

                // Push argument
                COMMAND_PUSH_ARGV((&CMD), strdup(buf));
                bufidx = 0;
                NEXT_TOKEN();

            case TOKEN_TYPE_STRING:
                if (!parser_quoted && !parser_single_quoted && !parser_pending_redirect && CMD.argc == 0 && bufidx == 0 && tok->value) {

                    int res = parser_checkToken(tok);
                    if (res == 1) {
                        free(tok->value);
                        goto _cleanup;
                    } else if (res == -1) {
                        // syntax error
                        goto _cleanup;
                    }
                }
                if (bufidx + strlen(tok->value) >= bufsz) BUFFER_GROW(); // TODO: Better grow check

                // Copy
                strncpy(buf + bufidx, tok->value, bufsz - bufidx);
                bufidx += strlen(tok->value);
                free(tok->value);
                NEXT_TOKEN();

            case TOKEN_TYPE_DOUBLE_QUOTE:
                parser_quoted = !(parser_quoted);
                break;

            case TOKEN_TYPE_SINGLE_QUOTE:
                if (parser_quoted && !parser_single_quoted) {
                    BUFFER_PUSH('\'');
                    NEXT_TOKEN();
                }

                parser_single_quoted = !(parser_single_quoted);
                parser_quoted = !(parser_quoted);
                NEXT_TOKEN();

            case TOKEN_TYPE_REDIRECT_OUT:
                if (parser_quoted) {
                    BUFFER_PUSH('>');
                    NEXT_TOKEN();
                }

                // Create a redirection
                parser_pending_fd = STDOUT_FILENO;
                parser_pending_redirect = 1;

                // Consume spaces
                token_t *spc = lexer_getToken(tok);
                while (spc->type == TOKEN_TYPE_SPACE) {
                    token_t *next = lexer_getToken(spc);
                    free(spc);
                    spc = next;
                }

                lexer_ungetToken(spc);

                NEXT_TOKEN();

            case TOKEN_TYPE_OR:
                if (parser_quoted) {
                    BUFFER_PUSH('|');
                    BUFFER_PUSH('|');
                    NEXT_TOKEN();
                }

                // We must have at least one argument
                if (!CMD.argc || parser_pending_redirect) {
                    parser_syntaxError(tok);
                    goto _cleanup;
                }

                // Push existing buffer contents if required
                if (bufidx) {
                    COMMAND_PUSH_ARGV((&CMD), strdup(buf));
                    bufidx = 0;
                }

                // New command
                COMMAND_NEW(cmds, (cmd_count+1));
                cmd_count += 1;

                CMD.exec_flags |= COMMAND_FLAG_OR;

                NEXT_TOKEN();

            case TOKEN_TYPE_PIPE:
                // Single pipe (pipeline)
                if (parser_quoted) {
                    BUFFER_PUSH('|');
                    NEXT_TOKEN();
                }

                if (!CMD.argc || parser_pending_redirect) {
                    parser_syntaxError(tok);
                    goto _cleanup;
                }

                // Push pending buffer content
                if (bufidx) {
                    COMMAND_PUSH_ARGV((&CMD), strdup(buf));
                    bufidx = 0;
                }

                // Start next command and mark it as piped-from-previous
                COMMAND_NEW(cmds, (cmd_count+1));
                cmd_count += 1;
                CMD.exec_flags |= COMMAND_FLAG_PIPE_FROM_PREV;

                NEXT_TOKEN();

            case TOKEN_TYPE_AND:
                // AND token
                if (parser_quoted) {
                    BUFFER_PUSH('&');
                    BUFFER_PUSH('&');
                    NEXT_TOKEN();
                }

                // We must have at least one argument
                if (!CMD.argc || parser_pending_redirect) {
                    parser_syntaxError(tok);
                    goto _cleanup;
                }

                // Push existing buffer contents if required
                if (bufidx) {
                    COMMAND_PUSH_ARGV((&CMD), strdup(buf));
                    bufidx = 0;
                }

                // New command
                COMMAND_NEW(cmds, (cmd_count+1));
                cmd_count += 1;

                CMD.exec_flags |= COMMAND_FLAG_AND;

                NEXT_TOKEN();

            case TOKEN_TYPE_SEMICOLON:
                // Semicolon for command list    
                TOKEN_IGNORE_QUOTED(';');
                
                if (!CMD.argc && !bufidx) {
                    parser_syntaxError(tok);
                    goto _cleanup;
                }

                // Push existing buffer contents if required
                if (bufidx) {
                    COMMAND_PUSH_ARGV((&CMD), strdup(buf));
                    bufidx = 0;
                }

                // New command
                COMMAND_NEW(cmds, (cmd_count+1));
                cmd_count += 1;

                NEXT_TOKEN();

            case TOKEN_TYPE_EQUALS:
                // Equal sign, commonly used for environs
                TOKEN_IGNORE_QUOTED('=');
                if (CMD.argc || !bufidx) {
                    BUFFER_PUSH('=');
                    NEXT_TOKEN();
                }

                // Now we should keep processing entries to get the environmental variable name
                // TODO: Support for more additional token types?
                token_t *nxt = lexer_getToken(tok);

                buffer_t *env_buffer = buffer_create(128);

                while (nxt && nxt->type != TOKEN_TYPE_EOF && nxt->type != TOKEN_TYPE_NEWLINE) {
                    if (nxt->type == TOKEN_TYPE_SPACE && !parser_quoted) {
                        // We're done here
                        break;
                    } else if (nxt->type == TOKEN_TYPE_DOUBLE_QUOTE) {
                        // Double quote
                        if (parser_single_quoted) { buffer_push(env_buffer, '\"'); goto _equals_next_token; }
                        parser_quoted = !(parser_quoted);
                    } else if (nxt->type == TOKEN_TYPE_SINGLE_QUOTE) {
                        // Single quote
                        if (parser_quoted && !parser_single_quoted) { buffer_push(env_buffer, '\''); goto _equals_next_token; }
                        parser_quoted = !(parser_quoted);
                        parser_single_quoted = !(parser_single_quoted);
                    } else if (nxt->type == TOKEN_TYPE_STRING) {
                        // Regular string
                        buffer_pushString(env_buffer, nxt->value);
                        free(nxt->value);
                    } else if (nxt->type == TOKEN_TYPE_DOLLAR) {
                        if (parser_single_quoted) {
                            buffer_push(env_buffer, '$');
                            goto _equals_next_token;
                        }

                        char *var = parser_interpretVariable(nxt);

                        if (var) {
                            buffer_pushString(env_buffer, var);
                            free(var);
                        }
                    }

                _equals_next_token:
                    token_t *nxt2 = lexer_getToken(nxt);
                    free(nxt);
                    nxt = nxt2;
                }

                // Unget the last token
                if (nxt) lexer_ungetToken(nxt);

                // Append a command environ
                char *environ_statement = malloc(strlen(env_buffer->buffer) + 1 + strlen(buf) + 1);
                sprintf(environ_statement, "%s=%s", buf, env_buffer->buffer);

                // Destroy the environment buffer
                buffer_destroy(env_buffer);

                // Push the environment
                COMMAND_PUSH_ENVIRON(&CMD, environ_statement);

                // Reset buffer
                bufidx = 0;

                NEXT_TOKEN();

            case TOKEN_TYPE_NEWLINE:
            case TOKEN_TYPE_EOF:
                // Newline
                if (parser_pending_redirect) {
                    if (bufidx) {
                        parser_pending_redirect = 0;
    
                        // Yes, let's redirect!
                        // The data should be contained in buffer since we haven't pushed a new argv
                        // Open the file
    
                        int f = open(buf, (parser_pending_fd == STDIN_FILENO) ? O_RDONLY : O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
                        if (f < 0) {
                            fprintf(stderr, "essence: %s: %s\n", buf, strerror(errno));
                            
                            // TODO: Remove this command *properly*
                            cmd_count--;
    
                            goto _execute;
                        }
    
                        switch (parser_pending_fd) {
                            case STDIN_FILENO:
                                CMD.stdin = f;
                                break;
                            case STDOUT_FILENO:
                                CMD.stdout = f;
                                break;
                            case STDERR_FILENO:
                                CMD.stderr = f;
                                break;
                        }
    
                        bufidx = 0;
                        break;
                    }

                    // Error
                    parser_syntaxError(tok);
                    goto _cleanup;
                }

                if (bufidx) {
                    COMMAND_PUSH_ARGV((&CMD), strdup(buf));
                    bufidx = 0;
                }
                
                break;

            case TOKEN_TYPE_DOLLAR:
                // Dollar sign for environ
                if (parser_single_quoted) {
                    BUFFER_PUSH('$');
                    NEXT_TOKEN();
                }

                char *tmp = parser_interpretVariable(tok);
                if (tmp) {
                    char *p = tmp;
                    while (*p) { BUFFER_PUSH(*p); p++; }
                    free(tmp);                
                }


                NEXT_TOKEN();

            case TOKEN_TYPE_TILDE:
                TOKEN_IGNORE_QUOTED('~');
                
                // replace with environ
                char *home = getenv("HOME");
                if (!home) home = "/root/";

                char *p = home;
                while (*p) { BUFFER_PUSH(*p); p++; }
                
                NEXT_TOKEN();
                

            case TOKEN_TYPE_HASHTAG:
                TOKEN_IGNORE_QUOTED('#');

                // Consume all tokens
                token_t *n = lexer_getToken(tok);

                while (n && n->type != TOKEN_TYPE_NEWLINE && n->type != TOKEN_TYPE_EOF) {
                    if (n->type == TOKEN_TYPE_STRING) {
                        free(n->value);
                    }

                    token_t *n2 = lexer_getToken(n);
                    free(n);
                    n = n2;
                }

                lexer_ungetToken(n);
                break;

            default:
                fprintf(stderr, "essence: parser: Unrecognized token %d\n", tok->type);
                NEXT_TOKEN();
        }

        // Is this a newline/EOF?
        if (tok->type == TOKEN_TYPE_NEWLINE || tok->type == TOKEN_TYPE_EOF) {
            break;
        }
 
        // Next token
    _next_token:
    }

    // Execute the commands
_execute:
    command_executeList(cmds, cmd_count);

_cleanup:
    // Free the token
    if (tok) free(tok);

    for (int i = 0; i < cmd_count; i++) {
        command_cleanup(&cmds[i]);
    }

    // Free commands and buffer
    free(cmds);
    free(buf);

    return;
}
