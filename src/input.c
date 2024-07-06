#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "input.h"

#define CH_BACKSPACE 127
#define CSI_START_INTRO_1 27
#define CSI_START_INTRO_2 '['
#define CSI_UP 'A'
#define CSI_DOWN 'B'

struct sh_input_context {
    char *edit_buf; /** Buffer containing the text to edit and display. */
    size_t edit_buf_capacity; /**< Capacity of the edit buffer. */
    size_t edit_buf_len; /**< Number of characters in the edit buffer (excluding
                            the null byte). */
    size_t edit_buf_cursor; /**< Index of the cursor in the edit buffer. */

    char *new_cmdline;           /**< Buffer for saving a new command line. */
    size_t new_cmdline_capacity; /**< Capacity of the new commandline buffer. */
    size_t new_cmdline_len; /**< Number of characters in the new commandline
                               buffer (excluding the null byte). */

    struct sh_shell_context const *sh_ctx;
    size_t history_idx;
};

void init_input_context(
    struct sh_input_context *input_ctx,
    struct sh_shell_context const *sh_ctx
);

void handle_backspace(struct sh_input_context *input_ctx);

bool handle_csi_code(struct sh_input_context *input_ctx, char code);

bool handle_up(struct sh_input_context *input_ctx);

void handle_down(struct sh_input_context *input_ctx);

void insert_char(struct sh_input_context *input_ctx, char c);

void terminate_input(struct sh_input_context *input_ctx);

void destroy_input_context(struct sh_input_context *input_ctx);

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
    struct sh_shell_context const *sh_ctx,
    char **out,
    size_t *out_capacity
) {
    struct termios orig_termios;
    if (!enable_raw_mode(&orig_termios)) {
        return -1;
    }

    struct sh_input_context input_ctx;
    init_input_context(&input_ctx, sh_ctx);

    // Allocate memory for the edit buffer.
    {
        size_t new_edit_buf_capacity = 64;
        char *new_edit_buf = malloc(sizeof(char) * new_edit_buf_capacity);
        if (new_edit_buf == NULL) {
            perror("malloc");
            return -1;
        }
        input_ctx.edit_buf_capacity = new_edit_buf_capacity;
        input_ctx.edit_buf = new_edit_buf;
    }

    char c;
    while ((c = getchar()) != '\n') {
        // Grow the edit buffer if needed.
        if (input_ctx.edit_buf_len + 1 >= input_ctx.edit_buf_capacity) {
            size_t new_buf_capacity = input_ctx.edit_buf_capacity * 2;
            char *new_buffer = realloc(
                input_ctx.edit_buf,
                sizeof(char) * new_buf_capacity
            );
            if (new_buffer == NULL) {
                perror("realloc");
                goto err;
            }
            input_ctx.edit_buf = new_buffer;
            input_ctx.edit_buf_capacity = new_buf_capacity;
        }

        // Handle backspace.
        if (c == CH_BACKSPACE) {
            handle_backspace(&input_ctx);
            continue;
        }

        // Handle CSI escape sequences.
        if (c == CSI_START_INTRO_1 && (c = getchar()) == CSI_START_INTRO_2) {
            char code = getchar();
            handle_csi_code(&input_ctx, code);
            continue;
        }

        // Handle other characters.
        insert_char(&input_ctx, c);
    }

    // We only reach this point if the user enters a newline.
    assert(c == '\n');

    terminate_input(&input_ctx);

    restore_term_mode(&orig_termios);

    *out_capacity = input_ctx.edit_buf_capacity;
    *out = input_ctx.edit_buf;

    input_ctx.edit_buf = NULL;
    destroy_input_context(&input_ctx);

    return input_ctx.edit_buf_len;

err:
    destroy_input_context(&input_ctx);
    return -1;
}

void init_input_context(
    struct sh_input_context *input_ctx,
    struct sh_shell_context const *sh_ctx
) {
    *input_ctx = (struct sh_input_context) {
        .edit_buf = NULL,
        .edit_buf_capacity = 0,
        .edit_buf_len = 0,
        .edit_buf_cursor = 0,

        .new_cmdline = NULL,
        .new_cmdline_capacity = 0,
        .new_cmdline_len = 0,

        .sh_ctx = sh_ctx,
        .history_idx = sh_ctx->history_count,
    };
}

void handle_backspace(struct sh_input_context *input_ctx) {
    if (input_ctx->edit_buf_len > 0) {
        // "\b" moves the cursor back by one and does not actually erase
        // any characters. Hence, we use " " to overwrite the character
        // with a space so that it looks like it has been deleted. Since
        // " " was written, we need to move the cursor back again using
        // "\b".
        printf("\b \b");

        input_ctx->edit_buf_len--;
        input_ctx->edit_buf_cursor--;
    }
}

bool handle_csi_code(struct sh_input_context *input_ctx, char code) {
    switch (code) {
    case CSI_UP:
        return handle_up(input_ctx);
    case CSI_DOWN:
        handle_down(input_ctx);
        break;
    default:
        // Don't know how to handle, so do nothing.
        break;
    }

    return true;
}

bool handle_up(struct sh_input_context *input_ctx) {
    if (input_ctx->sh_ctx->history_count == 0 || input_ctx->history_idx == 0) {
        return true;
    }

    // Delete all characters on `stdout`.
    for (size_t idx = 0; idx < input_ctx->edit_buf_len; idx++) {
        printf("\b \b");
    }

    // If we're moving away from the new commandline, then we need
    // to save it.
    if (input_ctx->history_idx == input_ctx->sh_ctx->history_count) {
        if (input_ctx->new_cmdline_capacity < input_ctx->edit_buf_len + 1) {
            size_t new_buf_capacity = (input_ctx->edit_buf_len + 1) * 2;
            char *new_buffer = realloc(
                input_ctx->new_cmdline,
                sizeof(char) * new_buf_capacity
            );
            if (new_buffer == NULL) {
                perror("realloc");
                free(input_ctx->new_cmdline);
                return false;
            }
            input_ctx->new_cmdline = new_buffer;
            input_ctx->new_cmdline_capacity = new_buf_capacity;
        }
        strncpy(
            input_ctx->new_cmdline,
            input_ctx->edit_buf,
            input_ctx->edit_buf_len
        );
        input_ctx->new_cmdline_len = input_ctx->edit_buf_len;
        input_ctx->new_cmdline[input_ctx->new_cmdline_len] = '\0';
    }

    // Copy the previous line into the edit buffer.
    input_ctx->history_idx--;
    size_t len = strlen(input_ctx->sh_ctx->history[input_ctx->history_idx]);
    if (input_ctx->edit_buf_capacity < len + 1) {
        size_t new_buf_capacity = (len + 1) * 2;
        char *new_buffer = realloc(
            input_ctx->edit_buf,
            sizeof(char) * new_buf_capacity
        );
        if (new_buffer == NULL) {
            perror("realloc");
            free(input_ctx->edit_buf);
            return false;
        }
        input_ctx->edit_buf = new_buffer;
        input_ctx->edit_buf_capacity = new_buf_capacity;
    }

    strncpy(
        input_ctx->edit_buf,
        input_ctx->sh_ctx->history[input_ctx->history_idx],
        len
    );

    input_ctx->edit_buf_len = len;
    input_ctx->edit_buf_cursor = len;
    input_ctx->edit_buf[input_ctx->edit_buf_len] = '\0';

    // Replace the output on `stdout` with the previous line.
    printf("%s", input_ctx->edit_buf);

    return true;
}

void handle_down(struct sh_input_context *input_ctx) {
    // Down arrow.
    if (input_ctx->sh_ctx->history_count == 0
        || input_ctx->history_idx >= input_ctx->sh_ctx->history_count)
    {
        return;
    }

    // Delete all characters on `stdout`.
    for (size_t idx = 0; idx < input_ctx->edit_buf_len; idx++) {
        printf("\b \b");
    }

    // Copy the next line into the buffer.
    input_ctx->history_idx++;
    size_t len;
    char *history_line;
    if (input_ctx->history_idx == input_ctx->sh_ctx->history_count) {
        len = input_ctx->new_cmdline_len;
        history_line = input_ctx->new_cmdline;
    } else {
        len = strlen(input_ctx->sh_ctx->history[input_ctx->history_idx]);
        history_line = input_ctx->sh_ctx->history[input_ctx->history_idx];
    }

    // The only way to move back down is to have moved up before.
    // When we move up, we may grow the buffer. However, the buffer
    // will never shrink, so, when moving down, the buffer is
    // guaranteed to have enough space.
    assert(input_ctx->edit_buf_capacity >= len + 1);

    strncpy(input_ctx->edit_buf, history_line, len);
    input_ctx->edit_buf_len = len;
    input_ctx->edit_buf_cursor = len;
    input_ctx->edit_buf[input_ctx->edit_buf_len] = '\0';

    // Replace the outpu t on `stdout` with the next line.
    printf("%s", input_ctx->edit_buf);
}

void insert_char(struct sh_input_context *input_ctx, char c) {
    input_ctx->edit_buf[input_ctx->edit_buf_len] = c;
    input_ctx->edit_buf_len++;
    input_ctx->edit_buf_cursor++;

    // Write the character to `stdout`.
    putchar(c);
}

void terminate_input(struct sh_input_context *input_ctx) {
    input_ctx->edit_buf[input_ctx->edit_buf_len] = '\0';
    printf("\n");
}

void destroy_input_context(struct sh_input_context *input_ctx) {
    free(input_ctx->edit_buf);
    input_ctx->edit_buf = NULL;
    input_ctx->edit_buf_capacity = 0;
    input_ctx->edit_buf_len = 0;
    input_ctx->edit_buf_cursor = 0;

    free(input_ctx->new_cmdline);
    input_ctx->new_cmdline_capacity = 0;
    input_ctx->new_cmdline_len = 0;
}
