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

    // Buffer containing the text to edit and display.
    size_t edit_buf_capacity = 64;
    size_t edit_buf_idx = 0; // Index into the buffer.
    char *edit_buf = malloc(sizeof(char) * edit_buf_capacity);
    if (edit_buf == NULL) {
        perror("malloc");
        return -1;
    }

    // Buffer to save the new command line.
    // We need to keep track of this so that users can use the down arrow key to
    // go back to what they were entering before they had used the up arrow key
    // to navigate to a previous command line in the history.
    size_t new_cmdline_capacity = 0;
    size_t new_cmdline_idx = 0;
    char *new_cmdline = NULL;

    // Points to an entry in the history. However, if `history_idx` is equal to
    // `ctx->history_count`, then we take it as the new commandline.
    size_t history_idx = ctx->history_count;

    char c;
    while ((c = getchar()) != '\n') {
        // Grow the edit buffer if needed.
        if (edit_buf_idx >= edit_buf_capacity) {
            size_t new_buf_capacity = edit_buf_capacity * 2;
            char *new_buffer = realloc(
                edit_buf,
                sizeof(char) * new_buf_capacity
            );
            if (new_buffer == NULL) {
                perror("realloc");
                free(edit_buf);
                return -1;
            }
            edit_buf = new_buffer;
            edit_buf_capacity = new_buf_capacity;
        }

        // Handle backspace.
        if (c == 127) {
            if (edit_buf_idx > 0) {
                edit_buf_idx--;

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
                for (size_t idx = 0; idx < edit_buf_idx; idx++) {
                    printf("\b \b");
                }

                // If we're moving away from the new commandline, then we need
                // to save it.
                if (history_idx == ctx->history_count) {
                    if (new_cmdline_capacity < edit_buf_idx + 1) {
                        size_t new_buf_capacity = (edit_buf_idx + 1) * 2;
                        char *new_buffer = realloc(
                            new_cmdline,
                            sizeof(char) * new_buf_capacity
                        );
                        if (new_buffer == NULL) {
                            perror("realloc");
                            free(new_cmdline);
                            return -1;
                        }
                        new_cmdline = new_buffer;
                        new_cmdline_capacity = new_buf_capacity;
                    }
                    strncpy(new_cmdline, edit_buf, edit_buf_idx);
                    new_cmdline_idx = edit_buf_idx;
                    new_cmdline[new_cmdline_idx] = '\0';
                }

                // Copy the previous line into the buffer.
                history_idx--;
                size_t len = strlen(ctx->history[history_idx]);
                if (edit_buf_capacity < len + 1) {
                    size_t new_buf_capacity = (len + 1) * 2;
                    char *new_buffer = realloc(
                        edit_buf,
                        sizeof(char) * new_buf_capacity
                    );
                    if (new_buffer == NULL) {
                        perror("realloc");
                        free(edit_buf);
                        return -1;
                    }
                    edit_buf = new_buffer;
                    edit_buf_capacity = new_buf_capacity;
                }

                strncpy(edit_buf, ctx->history[history_idx], len);
                edit_buf_idx = len;
                edit_buf[edit_buf_idx] = '\0';

                // Replace the output on `stdout` with the previous line.
                printf("%s", edit_buf);
                continue;
            }

            // Down arrow.
            if (seq[0] == '[' && seq[1] == 'B' && ctx->history_count > 0
                && history_idx < ctx->history_count)
            {
                // Delete all characters on `stdout`.
                for (size_t idx = 0; idx < edit_buf_idx; idx++) {
                    printf("\b \b");
                }

                // Copy the next line into the buffer.
                history_idx++;
                size_t len;
                char *history_line;
                if (history_idx == ctx->history_count) {
                    len = new_cmdline_idx;
                    history_line = new_cmdline;
                } else {
                    len = strlen(ctx->history[history_idx]);
                    history_line = ctx->history[history_idx];
                }

                // The only way to move back down is to have moved up before.
                // When we move up, we may grow the buffer. However, the buffer
                // will never shrink, so, when moving down, the buffer is
                // guaranteed to have enough space.
                assert(edit_buf_capacity >= len + 1);

                strncpy(edit_buf, history_line, len);
                edit_buf_idx = len;
                edit_buf[edit_buf_idx] = '\0';

                // Replace the output on `stdout` with the next line.
                printf("%s", edit_buf);
                continue;
            }

            continue;
        }

        // Handle other characters.

        // Save the character to the buffer.
        edit_buf[edit_buf_idx] = c;
        edit_buf_idx++;

        // Write the character to `stdout`.
        putchar(c);
    }

    // We only reach this point if the user enters a newline.
    assert(c == '\n');
    edit_buf[edit_buf_idx] = '\0';
    printf("\n");

    restore_term_mode(&orig_termios);

    *out_capacity = edit_buf_capacity;
    *out = edit_buf;

    return edit_buf_idx;
}
