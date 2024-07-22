#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "input.h"

#define CH_BACKSPACE 127

#define C0_BACKSPACE 0x08
#define C0_START 0x00
#define C0_END 0x1F

#define CSI_START_INTRO_1 27
#define CSI_START_INTRO_2 '['
#define CSI_UP 'A'
#define CSI_DOWN 'B'
#define CSI_FORWARD 'C'
#define CSI_CPR 'R'

#define DSR_POS "6n"

struct sh_input_context {
    size_t cursor_line; /**< The line the cursor is on. */
    size_t cursor_col;  /**< The column the cursor is on. */

    unsigned short win_width;  /**< Width of the terminal window. */
    unsigned short win_height; /**< Height of the terminal window. */

    char *edit_buf; /**< Buffer containing the text to edit and display. */
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

char handle_csi(struct sh_input_context *input_ctx);

bool handle_up(struct sh_input_context *input_ctx);

void handle_down(struct sh_input_context *input_ctx);

bool handle_cpr(struct sh_input_context *input_ctx, char const *bytes);

void insert_char(struct sh_input_context *input_ctx, char c);

bool update_win_size(struct sh_input_context *input_ctx);

bool request_cursor_pos();

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

    update_win_size(&input_ctx);

    // Request the cursor's position.
    // Response from the terminal is retrieved from the CPR control sequence
    // (handled in `handle_csi()`).
    request_cursor_pos();

    char c;
    while ((c = getchar()) != '\n') {
        if (c == EOF) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("getchar");
            }
        }

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

        bool should_request_cursor_update = true;
        if (c == CSI_START_INTRO_1 && (c = getchar()) == CSI_START_INTRO_2) {
            // Handle CSI control sequences.
            // We don't want to request for a cursor update if we just handled
            // `CSI_CPR` since that would cause an infinite loop.
            if (handle_csi(&input_ctx) == CSI_CPR) {
                should_request_cursor_update = false;
            }
        } else if (c == CH_BACKSPACE || c == C0_BACKSPACE) {
            // Handle backspace.
            handle_backspace(&input_ctx);
        } else if (c >= C0_START && c <= C0_END) {
            // Ignore other C0 control codes.
        } else {
            // Handle other characters.
            insert_char(&input_ctx, c);
        }

        if (should_request_cursor_update) {
            request_cursor_pos();
        }

        fflush(stdout);
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
        // These are 1-indexed!
        .cursor_line = 0,
        .cursor_col = 0,

        .win_width = 0,
        .win_height = 0,

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
    if (input_ctx->edit_buf_len <= 0) {
        return;
    }

    // If the cursor is at the first column, backspace should first move the
    // cursor up by one line and all the way to the right.
    if (input_ctx->cursor_col == 1) {
        // Move the cursor up by one line.
        printf("%c%c%c", CSI_START_INTRO_1, CSI_START_INTRO_2, CSI_UP);

        // It is not easy to get the width of the terminal. However, we  can
        // just move the cursor to the right by a large enough number since
        // well-behaved terminals will prevent the cursor from "going off".
        printf(
            "%c%c%d%c",
            CSI_START_INTRO_1,
            CSI_START_INTRO_2,
            1024,
            CSI_FORWARD
        );

        // Finally, we erase the character at the current position.
        printf(" ");

        // After inserting a character at the last column, the terminal gets
        // into a weird state where the cursor is at the last column, but
        // printing a character will move the cursor to the next line and
        // display the character there (i.e., at the first column of the next
        // line). Since we printed a " " at the last column, this behaviour will
        // occur. Moving the cursor forward (even though it is at the last
        // column) seems to remove this state.
        printf("%c%c%c", CSI_START_INTRO_1, CSI_START_INTRO_2, CSI_FORWARD);

        input_ctx->cursor_col = input_ctx->win_width;
        input_ctx->cursor_line--;
    } else {
        // "\b" moves the cursor back by one and does not actually erase
        // any characters. Hence, we use " " to overwrite the character
        // with a space so that it looks like it has been deleted. Since
        // " " was written, we need to move the cursor back again using
        // "\b".
        printf("\b \b");

        input_ctx->cursor_col--;
    }

    input_ctx->edit_buf_len--;
    input_ctx->edit_buf_cursor--;
}

char handle_csi(struct sh_input_context *input_ctx) {
    char buf[32]; // Should be more than large enough.
    size_t idx = 0;
    char c = '\0';
    do {
        c = getchar();
        if (c == EOF) {
            return false;
        }

        buf[idx] = c;
        idx++;

        // The buffer was too small, so we need to clean up a little. Extremely
        // unlikely to happen.
        if (idx >= sizeof(buf)) {
            // Consume all the input characters until the next alphabetical
            // character.
            while (!(c >= 'A' && c <= 'z')) {
                c = getchar();
            }
            return false;
        }
    } while (!(c >= 'A' && c <= 'z'));
    // We will assume that sequences are ended with an alphabetical
    // character (seems to be the case).

    buf[idx] = '\0';

    switch (buf[idx - 1]) {
    case CSI_UP:
        handle_up(input_ctx);
        return CSI_UP;
    case CSI_DOWN:
        handle_down(input_ctx);
        return CSI_DOWN;
    case CSI_CPR:
        handle_cpr(input_ctx, buf);
        return CSI_CPR;
        break;
    default:
        // Don't know how to handle, so do nothing.
        return '\0';
    }
}

bool handle_up(struct sh_input_context *input_ctx) {
    if (input_ctx->sh_ctx->history_count == 0 || input_ctx->history_idx == 0) {
        return true;
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

    // Delete all characters on `stdout`.
    size_t char_count = input_ctx->edit_buf_len;
    for (size_t idx = 0; idx < char_count; idx++) {
        handle_backspace(input_ctx);
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
    if (input_ctx->sh_ctx->history_count == 0
        || input_ctx->history_idx >= input_ctx->sh_ctx->history_count)
    {
        return;
    }

    // Delete all characters on `stdout`.
    size_t char_count = input_ctx->edit_buf_len;
    for (size_t idx = 0; idx < char_count; idx++) {
        handle_backspace(input_ctx);
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

    // Replace the output on `stdout` with the next line.
    printf("%s", input_ctx->edit_buf);
}

bool handle_cpr(struct sh_input_context *input_ctx, char const *bytes) {
    // Parse the line and column numbers.
    size_t line;
    size_t col;
    if (sscanf(bytes, "%lu;%lu", &line, &col) != 2) {
        return false;
    }

    input_ctx->cursor_line = line;
    input_ctx->cursor_col = col;
    return true;
}

void insert_char(struct sh_input_context *input_ctx, char c) {
    input_ctx->edit_buf[input_ctx->edit_buf_len] = c;
    input_ctx->edit_buf_len++;
    input_ctx->edit_buf_cursor++;

    // Write the character to `stdout`.
    putchar(c);

    // Check if the cursor is at the last column.
    update_win_size(input_ctx);
    if (input_ctx->cursor_col == input_ctx->win_width) {
        // Move the cursor down by one line and to the first column.
        printf("\n");
    }
}

bool update_win_size(struct sh_input_context *input_ctx) {
    struct winsize winsz;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &winsz) < 0) {
        return false;
    }

    input_ctx->win_width = winsz.ws_col;
    input_ctx->win_height = winsz.ws_row;
    return true;
}

bool request_cursor_pos() {
    return printf("%c%c%s", CSI_START_INTRO_1, CSI_START_INTRO_2, DSR_POS) == 4;
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
