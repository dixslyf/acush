#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "input.h"

bool enable_raw_mode(struct termios *orig_termios) {
    if (tcgetattr(STDIN_FILENO, orig_termios) < 0) {
        perror("tcgetattr");
        return false;
    }

    struct termios raw = *orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr");
        return false;
    }

    return true;
}

bool restore_term_mode(struct termios const *orig_termios) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios) < 0) {
        perror("tcsetattr");
        return false;
    }
    return true;
}

ssize_t read_input(
    struct sh_shell_context const *ctx,
    char **out,
    size_t *out_capacity
) {
    struct termios orig_termios;
    if (!enable_raw_mode(&orig_termios)) {
        return -1;
    }

    size_t buf_capacity = 64;
    size_t buf_idx = 0; // Index into the buffer.
    char *buf = malloc(sizeof(char) * buf_capacity);
    if (buf == NULL) {
        perror("malloc");
        return -1;
    }

    size_t history_idx = ctx->history_count;
    char c;
    while ((c = getchar()) != '\n') {
        // Grow the buffer if needed.
        if (buf_idx >= buf_capacity) {
            size_t new_buf_capacity = buf_capacity * 2;
            char *new_buffer = realloc(buf, sizeof(char) * new_buf_capacity);
            if (new_buffer == NULL) {
                perror("realloc");
                free(buf);
                return -1;
            }
            buf = new_buffer;
            buf_capacity = new_buf_capacity;
        }

        // Handle backspace.
        if (c == 127) {
            if (buf_idx > 0) {
                buf_idx--;

                // "\b" moves the cursor back by one and does not actually erase
                // any characters. Hence, we use " " to overwrite the character
                // with a space so that it looks like it has been deleted. Since
                // " " was written, we need to move the cursor back again using
                // "\b".
                printf("\b \b");
            }
            continue;
        }

        // Handle escape sequences.
        if (c == 27) {
            char seq[3];
            seq[0] = getchar();
            seq[1] = getchar();
            seq[2] = '\0';

            // Up arrow.
            if (seq[0] == '[' && seq[1] == 'A' && ctx->history_count > 0
                && history_idx > 0)
            {
                // Delete all characters on `stdout`.
                for (size_t idx = 0; idx < buf_idx; idx++) {
                    printf("\b \b");
                }

                // Copy the previous line into the buffer.
                history_idx--;
                size_t len = strlen(ctx->history[history_idx]);
                if (buf_capacity < len + 1) {
                    size_t new_buf_capacity = (len + 1) * 2;
                    char *new_buffer = realloc(
                        buf,
                        sizeof(char) * new_buf_capacity
                    );
                    if (new_buffer == NULL) {
                        perror("realloc");
                        free(buf);
                        return -1;
                    }
                    buf = new_buffer;
                    buf_capacity = new_buf_capacity;
                }

                strncpy(buf, ctx->history[history_idx], len);
                buf_idx = len;
                buf[buf_idx] = '\0';

                // Replace the output on `stdout` with the previous line.
                printf("%s", buf);
            }

            // Down arrow.
            if (seq[0] == '[' && seq[1] == 'B' && ctx->history_count > 0
                && history_idx < ctx->history_count - 1)
            {
                // Delete all characters on `stdout`.
                for (size_t idx = 0; idx < buf_idx; idx++) {
                    printf("\b \b");
                }

                // Copy the next line into the buffer.
                history_idx++;
                size_t len = strlen(ctx->history[history_idx]);
                if (buf_capacity < len + 1) {
                    size_t new_buf_capacity = (len + 1) * 2;
                    char *new_buffer = realloc(
                        buf,
                        sizeof(char) * new_buf_capacity
                    );
                    if (new_buffer == NULL) {
                        perror("realloc");
                        free(buf);
                        return -1;
                    }
                    buf = new_buffer;
                    buf_capacity = new_buf_capacity;
                }

                strncpy(buf, ctx->history[history_idx], len);
                buf_idx = len;
                buf[buf_idx] = '\0';

                // Replace the output on `stdout` with the next line.
                printf("%s", buf);
            }

            continue;
        }

        // Handle other characters.

        // Save the character to the buffer.
        buf[buf_idx] = c;
        buf_idx++;

        // Write the character to `stdout`.
        putchar(c);
    }

    // We only reach this point if the user enters a newline.
    assert(c == '\n');
    buf[buf_idx] = '\0';
    printf("\n");

    restore_term_mode(&orig_termios);

    *out_capacity = buf_capacity;
    *out = buf;

    return buf_idx;
}
