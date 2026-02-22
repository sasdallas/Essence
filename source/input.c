/**
 * @file input.c
 * @brief Essence input daemon
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 * 
 * @ref Cursor drawing functions, https://github.com/tayoky/tash/blob/main/src/prompt.c
 */

#include "essence.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <pwd.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdbool.h>

/* Input type */
int essence_input_type = INPUT_TYPE_INTERACTIVE;

/* Prompt type */
int essence_prompt = INPUT_PROMPT_PS1;

/* Prompt X */
int essence_prompt_x = 0;

/* Unread character */
int essence_unread_character = 0;

/* Buffer */
char *input_buffer = NULL;
size_t input_buffer_size = INPUT_DEFAULT_BUFFER_SIZE;
size_t input_buffer_idx = 0;
size_t input_buffer_len = 0;

/* Saved input buffer */
char *saved_input_buffer = NULL;
int history_index = 0;

/* Original termios settings */
static struct termios essence_original_termios;
static struct termios essence_new_termios;
static int essence_termios_ready = 0;

/* Input file */
FILE *input_script = NULL;

/**
 * @brief Parse PS prompt
 * @param prompt The prompt to parse
 */
char *input_parsePS(char *prompt) {
    buffer_t *buf = buffer_create(strlen(prompt));
    
    // I used https://ss64.com/bash/syntax-prompt.html as my reference for PS-syntax.
    char *p = prompt;

    // Get time
    time_t t;

    time(&t);
    struct tm *tm = localtime(&t);

    // Temporary buffer
    char tmp[128];

    while (*p) {
        if (*p == '\\') {
            // Next character
            p++;

            if (!*p) {
                buffer_push(buf, '\\');
                continue;
            }

            switch (*p) {
                case 'd':
                    // Date, in weekday month date
                    // Get date
                    strftime(tmp, 128, "%a %B %d", tm);

                    buffer_pushString(buf, tmp);
                    break;

                case 'h':
                case 'H':
                    // Get hostname
                    gethostname(tmp, 128);

                    // \h is for hostname up to the first .
                    if (*p == 'h') {
                        char *dot = strchr(tmp, '.');
                        if (dot) *tmp = 0;
                    } 

                    buffer_pushString(buf, tmp);
                    break;

                case 'j':
                    // Currently running jobs
                    buffer_push(buf, '0');
                    break;

                case 's':
                    // Name of the shell
                    buffer_pushString(buf, "essence");
                    break;

                case 't':
                    // Time in 24-hour HH:MM:SS format
                    strftime(tmp, 128, "%H:%M:%S", tm);
                    buffer_pushString(buf, tmp);
                    break;

                case 'T':
                    // Time in 12-hour HH:MM:SS
                    strftime(tmp, 128, "%I:%M:%S", tm);
                    buffer_pushString(buf, tmp);
                    break;
                
                case '@':
                    // Time in 12-hour am/pm format
                    strftime(tmp, 128, "%I:%M %p", tm);
                    buffer_pushString(buf, tmp);
                    break;

                case 'u':
                    // Username of the current user
                    struct passwd *p = getpwuid(geteuid());

                    if (p) {
                        buffer_pushString(buf, p->pw_name);
                    } else {
                        buffer_pushString(buf, "This-user-does-not-exist");
                    }

                    break;

                case '$':
                    if (!geteuid()) {
                        buffer_push(buf, '#');
                    } else {
                        buffer_push(buf, '$');
                    }

                    break;

                case 'W': ;
                    char cwd[256];
                    getcwd(cwd, 256);
                    char *s = strrchr(cwd, '/');
                    if (s) s = s + 1;
                    if (!s) s = cwd;
                    if (strlen(cwd) == 1) s = cwd;

                    buffer_pushString(buf, s);
                    break;


                case 'v':
                case 'V':
                    // Version
                    snprintf(tmp, 128, "%d.%d.%d", ESSENCE_VERSION_MAJOR, ESSENCE_VERSION_MINOR, ESSENCE_VERSION_LOWER);;
                    buffer_pushString(buf, tmp);
                    break;
                    
                case 'e':
                    buffer_push(buf, '\033');
                    break;

                case '+':
                    if (cmd_last_exit_status) {
                        buffer_pushString(buf, "\033[31m");
                    } else {
                        buffer_pushString(buf, "\033[32m");
                    }
            }


        } else {
            buffer_push(buf, *p);
        }

        p++;
    }


    char *b = buf->buffer;
    free(buf);
    return b;
}


/**
 * @brief Get the prompt
 */
char *input_getPrompt() {
    char *ps = (essence_prompt == INPUT_PROMPT_PS1) ? getenv("PS1") : getenv("PS2");

    if (!ps) {
        // Handle a fallback since $PS1 and $PS2 are undefined
        switch (essence_prompt) {
            case INPUT_PROMPT_PS1:
                // Fallback
                ps = "essence-fallback# ";
                break;
            case INPUT_PROMPT_PS2:
            default:    
                ps = "> "; 
                break;
        }
    }

    return input_parsePS(ps);
}

/**
 * @brief Cleanup function for interactive input
 */
void input_restoreInteractive() {
    if (essence_pid == getpid()) {
        tcsetattr(STDIN_FILENO, TCSANOW, &essence_original_termios);
    }
}

/**
 * @brief Redraw cursor
 */
static void input_redrawCursor() {
    // Thank you tayoky for the cursor code I could steal :D
    for (int x = essence_prompt_x; x < input_buffer_len; x++) {
        putchar(input_buffer[x]);
    }

    putchar(' ');
    putchar('\b');

    // Now go back...
    for (int x = essence_prompt_x; x < input_buffer_len; x++) {
        putchar('\b');
    }

    fflush(stdout);
}

static void add_match(char ***list, size_t *count, size_t *cap, const char *entry) {
    if (*count + 1 >= *cap) {
        *cap *= 2;
        *list = realloc(*list, *cap * sizeof(char*));
    }
    (*list)[(*count)++] = strdup(entry);
}

char **autocomplete(char *str, bool first_token) {
    size_t cap = 32, count = 0;
    char **out = malloc(sizeof(char*) * cap);
    if (!out) return NULL;

    char *input = strdup(str); 
    char *dir = ".";
    char *prefix = input;

    if (input[0] == '~') {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            char tmp[4096];
            snprintf(tmp, sizeof(tmp), "%s%s", pw->pw_dir, input+1);
            free(input);
            input = strdup(tmp);
        }
    }

    char *slash = strrchr(input, '/');
    if (slash) {
        *slash = '\0';
        dir = (*input ? input : "/");
        prefix = slash + 1;
    }

    size_t prelen = strlen(prefix);

    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            if (strncmp(e->d_name, prefix, prelen) == 0) {
                char full[4096];
                snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);

                struct stat st;
                if (stat(full, &st) == 0) {
                    char formatted[4096];
                    if (S_ISDIR(st.st_mode)) {
                        snprintf(formatted, sizeof(formatted), "%s/", e->d_name);
                    } else {
                        snprintf(formatted, sizeof(formatted), "%s", e->d_name);
                    }

                    add_match(&out, &count, &cap, formatted);
                }
            }
        }
        closedir(d);

        if (!first_token) {
            out[count] = NULL;
            free(input);
            return out;
        }
    }

    if (!slash) {
        char *env = getenv("PATH");
        if (env) {
            char *paths = strdup(env);
            char *pd = strtok(paths, ":");
            while (pd) {
                DIR *p = opendir(pd);
                if (p) {
                    struct dirent *e;
                    while ((e = readdir(p)) != NULL) {
                        if (e->d_name[0] == '.') continue;
                        if (strncmp(e->d_name, prefix, prelen) == 0) {
                            char full[4096];
                            snprintf(full, sizeof(full), "%s/%s", pd, e->d_name);

                            struct stat st;
                            if (stat(full, &st) == 0 && (st.st_mode & S_IXUSR)) {
                                add_match(&out, &count, &cap, e->d_name);
                            }
                        }
                    }
                    closedir(p);
                }
                pd = strtok(NULL, ":");
            }
            free(paths);
        }
    }

    out[count] = NULL;
    free(input);
    return out;
}


 
/**
 * @brief Get input (from stdin)
 * @param prompt Optional prompt to use
 */
char *input_getInteractive(char *user_prompt) {
    if (!essence_termios_ready) {
        tcgetattr(STDIN_FILENO, &essence_original_termios);
        tcgetattr(STDIN_FILENO, &essence_new_termios);

        essence_new_termios.c_lflag &= ~(ECHO | ICANON);
        atexit(input_restoreInteractive);

        tcsetattr(STDIN_FILENO, TCSANOW, &essence_new_termios);

        setvbuf(stdout, NULL, _IONBF, 0);

        essence_termios_ready = 1;
    }

    // Get the prompt
    char *prompt = user_prompt ? user_prompt : input_getPrompt();
    size_t prompt_length = strlen(prompt);

    // Print the prompt out
    printf("%s", prompt);
    fflush(stdout);

    // Get some characters
    int bksp = (int)essence_original_termios.c_cc[VERASE];

    // Reset input buffer + friends
    input_unloadBuffer();
    input_buffer = malloc(INPUT_DEFAULT_BUFFER_SIZE);
    input_buffer_size = INPUT_DEFAULT_BUFFER_SIZE;

    // Clear input buffer
    memset(input_buffer, 0, 512);

    int last_was_tab = 0;

    // Enter main loop
    while (1) {
        int ch = getchar();
        if (ch != '\t') last_was_tab = 0;

        // Handle ANSI arrow keys
        if (ch == '\033') {
            // Get next character
            int next_ch = getchar();

            if (next_ch != '[') {
                // Invalid ANSI escape sequence
                ungetc(next_ch, stdin);
                continue;
            }

            // Valid ANSI sequence, of what?
            next_ch = getchar();
            switch (next_ch) {
                case 'D':
                    // Left arrow key
                    if (essence_prompt_x) {
                        putchar('\b');
                        fflush(stdout);

                        essence_prompt_x--;
                    } else {
                        putchar('\a'); // Bell
                    }

                    continue;

                case 'C':
                    if (essence_prompt_x < strlen(input_buffer)) {
                        putchar(input_buffer[essence_prompt_x]);
                        fflush(stdout);

                        essence_prompt_x++;
                    }

                    continue;

                case 'A':
                    char *h = history_get(history_index);
                    if (!h) continue;

                    if (!history_index) {
                        saved_input_buffer = input_buffer;
                    }

                    // Calculate space difference
                    int diff = strlen(input_buffer) - strlen(h);

                    // Free old buffer if needed
                    if (history_index) free(input_buffer);

                    input_buffer = malloc(INPUT_DEFAULT_BUFFER_SIZE);
                    strcpy(input_buffer, h);

                    history_index++;

                    essence_prompt_x = strlen(h);
                    input_buffer_len = strlen(h);
                    input_buffer_size = strlen(h);

                    printf("\033[G%s%s", prompt, input_buffer);

                    if (diff) {
                        for (int i = 0; i < diff; i++) putchar(' ');
                        for (int i = 0; i < diff; i++) putchar('\b');
                    }

                    fflush(stdout);
                    
                    continue;
                
                case 'B':
                    if (!history_index) {
                        continue;
                    }

                    // Free old input buffer, lower index
                    char *old = input_buffer;
                    history_index--;

                    // Restore saved
                    if (!history_index) {
                        input_buffer = saved_input_buffer;
                        saved_input_buffer = NULL;
                    } else {
                        input_buffer = malloc(INPUT_DEFAULT_BUFFER_SIZE);
                        strcpy(input_buffer, history_get(history_index));
                    }

                    // Calculate space
                    diff = strlen(old) - strlen(input_buffer);
                    free(old);

                    
                    essence_prompt_x = strlen(input_buffer);
                    input_buffer_len = strlen(input_buffer);

                    printf("\033[G%s%s", prompt, input_buffer);

                    if (diff) {
                        for (int i = 0; i < diff; i++) putchar(' ');
                        for (int i = 0; i < diff; i++) putchar('\b');
                    }

                    fflush(stdout);
                    
                    continue;


                    
            }
        }

        if (ch == bksp) {
            // Backspace character, can we even go back?
            if (essence_prompt_x) {
                essence_prompt_x--;

                // Put character back in the bfufer
                memmove(&input_buffer[essence_prompt_x], &input_buffer[essence_prompt_x+1], input_buffer_len-essence_prompt_x);
                
                // Backspace
                putchar('\b');
                putchar(' ');
                putchar('\b');

                input_buffer_len--;

                // Redraw
                input_redrawCursor();
            }

            continue;
        }

        // What character is it
        switch (ch) {
            case '\n':
                // We are at the end of the file/line
                putchar('\n');

                input_buffer[input_buffer_len++] = '\n';

                // Do we need to enlarge the buffer?
                if (input_buffer_len >= input_buffer_size) {
                    input_buffer = realloc(input_buffer, input_buffer_size * 2);
                    input_buffer_size *= 2;
                }

                input_buffer[input_buffer_len] = 0;

                essence_prompt_x = 0;

                if (input_buffer_len) history_append(input_buffer);
                if (saved_input_buffer) free(saved_input_buffer);
                if (prompt && !user_prompt) free(prompt); 

                return input_buffer;
        
            case '\t':
                // Tab for auto completion
                // Find the last token (after the last space)

                // Temporarily null-terminate the token to pass to autocomplete
                char saved = input_buffer[essence_prompt_x];
                input_buffer[essence_prompt_x] = 0;
                
                // find start of last token (after last space or '/') 
                char *last_token = NULL;
                size_t start = essence_prompt_x;
                while (start > 0 && input_buffer[start-1] != ' ') start--;
                last_token = &input_buffer[start];

                char **s = autocomplete(last_token, (start == 0));
                input_buffer[essence_prompt_x] = saved; // restore original char

                if (*(s+1) == NULL) {
                    char *res = *s;

                    // find start
                    size_t start = essence_prompt_x;
                    while (start > 0 && input_buffer[start-1] != ' ' && input_buffer[start-1] != '/') start--;

                    size_t typed_len = essence_prompt_x - start; 

                    // compare
                    size_t common = 0;
                    while (res[common] && common < typed_len && res[common] == input_buffer[start + common]) common++;

                    const char *suffix = res + common; 

                    int trail = !(res[strlen(res)-1] == '/');
                    size_t insert_len = strlen(suffix) + trail; // account for space

                    while (input_buffer_len + insert_len + 1 >= input_buffer_size) {
                        input_buffer_size *= 2;
                        input_buffer = realloc(input_buffer, input_buffer_size);
                    }

                    memmove(&input_buffer[essence_prompt_x + insert_len], &input_buffer[essence_prompt_x], input_buffer_len - essence_prompt_x + 1);

                    if (trail != 0) {
                        memcpy(&input_buffer[essence_prompt_x], suffix, insert_len-1);
                        input_buffer[essence_prompt_x + insert_len-1] = ' ';  // trailing space
                    } else {
                        memcpy(&input_buffer[essence_prompt_x], suffix, insert_len);
                    }

                    input_buffer_len += insert_len;
                    essence_prompt_x  += insert_len;

                    for (size_t i = essence_prompt_x - insert_len; i < input_buffer_len; i++)
                        putchar(input_buffer[i]);

                    for (size_t i = 0; i < input_buffer_len - essence_prompt_x; i++)
                        putchar('\b');

                    fflush(stdout);
                } else {
                    if (last_was_tab) {
                        printf("\n");
                        char **p = s;
                        while (*p) {
                            printf("%s%s", *p, *(p+1) ? ", " : "");
                            p++;
                        }

                        input_buffer[essence_prompt_x] = saved;
                        printf("\n");
                        printf("%s", prompt);

                        if (input_buffer_len) {

                            for (int x = 0; x < input_buffer_len; x++) {
                                putchar(input_buffer[x]);
                            }

                            for (int x = essence_prompt_x; x < input_buffer_len - 1; x++) {
                                putchar('\b');
                            }
                        }
                        fflush(stdout);
                        last_was_tab = 0;
                    } else {
                        last_was_tab = 1;
                    }
                };

                char **p = s;
                while (*p) { free(*p); p++; }
                free(s);

                break;

            default:
                // Check if we need to enlarge the buffer first
                if (input_buffer_len + 1 >= input_buffer_size) {
                    char *new_buffer = realloc(input_buffer, input_buffer_size * 2);
                    if (!new_buffer) {
                        perror("realloc failed");
                        exit(1);
                    }
                    input_buffer = new_buffer;
                    input_buffer_size *= 2;
                }

                // Shift characters to the right to make space for new character
                memmove(&input_buffer[essence_prompt_x + 1],
                        &input_buffer[essence_prompt_x],
                        input_buffer_len - essence_prompt_x);

                // Insert the new character
                input_buffer[essence_prompt_x] = ch;
                input_buffer_len++;

                // Print the characters from cursor position to the end
                for (int x = essence_prompt_x; x < input_buffer_len; x++) {
                    putchar(input_buffer[x]);
                }

                // Move cursor back to the correct position
                for (int x = essence_prompt_x + 1; x < input_buffer_len; x++) {
                    putchar('\b');
                }

                essence_prompt_x++;

                // Null-terminate the string
                input_buffer[input_buffer_len] = '\0';
                break;
        }
    }
}

/**
 * @brief Get a input from a file
 */
char *input_getScript() {
    // Setup input buffer
    if (input_buffer) free(input_buffer);
    input_buffer = malloc(INPUT_DEFAULT_BUFFER_SIZE);
    input_buffer_size = INPUT_DEFAULT_BUFFER_SIZE;
    input_buffer_idx = 0;
    input_buffer_len = 0;
    input_buffer[0] = 0;

    // Read from the file
    char *p = fgets(input_buffer, input_buffer_size, input_script);
    if (!p) {
        input_buffer_len = 1;
        input_buffer[0] = EOF;
    } else {
        input_buffer_len = strlen(input_buffer);
    }

    // restore.. \n?
    input_buffer[input_buffer_len] = '\n';
    input_buffer[input_buffer_len+1] = 0;

    // Send input buffer
    return input_buffer;
}

/**
 * @brief Get a line of input from the input source
 * @param prompt Optional prompt to use
 */
char *input_get(char *user_prompt) {
    switch (essence_input_type) {
        case INPUT_TYPE_INTERACTIVE:
            return input_getInteractive(user_prompt);
        case INPUT_TYPE_SCRIPT:
            return input_getScript();
        default:
            printf("ERROR: Unknown input type %d\n", essence_input_type);
            return NULL;
    }
}

/**
 * @brief Get a character from input
 */
int input_getCharacter() {
    if (essence_unread_character) {
        int ch = essence_unread_character;
        essence_unread_character = 0;
        return ch;
    }

    if (!input_buffer) return 0;
    if (input_buffer_idx > input_buffer_len) return 0;

    return input_buffer[input_buffer_idx++];    
}

/**
 * @brief Unget a character from input
 * @param ch The character to unget
 */
void input_ungetCharacter(int ch) {
    essence_unread_character = ch;
}

/**
 * @brief Initialize input
 */
void input_init() {
    if (essence_input_type == INPUT_TYPE_INTERACTIVE) {
        // We are interactive input, load history
        history_load();
    }
}

/**
 * @brief Load a script for input
 * @param filename The script filename to load
 * @returns 0 on success
 */
int input_loadScript(char *filename) {
    input_unloadBuffer();



    input_script = fopen(filename, "r");
    if (!input_script) {
        fprintf(stderr, "essence: %s: %s\n", filename, strerror(errno));
        return 1;
    }

    // Load script
    essence_input_type = INPUT_TYPE_SCRIPT;
    return 0;
}

/**
 * @brief Switch to interactive
 */
void input_switchInteractive() {
    essence_input_type = INPUT_TYPE_INTERACTIVE;
}

/**
 * @brief Load buffer
 * @param buffer The buffer to load
 */
int input_loadBuffer(char *buffer) {
    input_unloadBuffer();

    char buf[512];
    snprintf(buf, 512, "%s\n", buffer);

    // Setup buffer parameters
    input_buffer = strdup(buf);
    input_buffer_len = strlen(buf);
    input_buffer_size = 512;
    input_buffer_idx = 0;
    essence_unread_character = 0;
    return 0;
}

/**
 * @brief Unload buffer
 */
void input_unloadBuffer() {
    if (input_buffer) {
        free(input_buffer);
    }

    input_buffer = NULL;
    input_buffer_idx = 0;
    input_buffer_size = 0;
    input_buffer_len = 0;
    essence_prompt_x = 0;
    saved_input_buffer = NULL;
    history_index = 0;
}