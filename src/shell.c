#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "run.h"
#include "shell.h"

#define STOP_SIGNALS_SIZE 3
static int const STOP_SIGNALS[STOP_SIGNALS_SIZE] = {SIGINT, SIGQUIT, SIGTSTP};

/** Represents the possible results of initializing the shell context. */
enum sh_init_shell_context_result {
    SH_INIT_SHELL_CONTEXT_SUCCESS,     /**< Successful initialization. */
    SH_INIT_SHELL_CONTEXT_MEMORY_ERROR /**< Memory allocation error. */
};

/**
 * Initializes the shell context.
 *
 * @param ctx a pointer to the shell context to initialise
 * @return the result of the initialization
 */
enum sh_init_shell_context_result
init_shell_context(struct sh_shell_context *ctx);

/**
 * Represents the possible results for adding a line to the shell's
 * command history.
 */
enum sh_add_to_history_result {
    SH_ADD_TO_HISTORY_SUCCESS,     /**< Successful addition to history. */
    SH_ADD_TO_HISTORY_MEMORY_ERROR /**< Memory allocation error. */
};

/**
 * Adds a line to the shell history.
 *
 * @param ctx a pointer to the shell context
 * @param line the command line to be added to history
 * @return the result of the addition to history
 */
enum sh_add_to_history_result
add_to_history(struct sh_shell_context *ctx, char *line);

/**
 * Destroys the shell context by freeing allocated resources.
 *
 * @param ctx a pointer to the shell context
 */
void destroy_shell_context(struct sh_shell_context *ctx);

/** Sets up signal handling for the shell. */
void setup_signals();

/**
 * Handler for the `SIGCHLD` signal.
 *
 * This handler consumes background processes with `waitpid()` to make sure they
 * do not become zombie processes.
 */
void handle_sigchld(int signo);

void enable_raw_mode(struct termios *orig_termios);

void disable_raw_mode(struct termios *orig_termios);

ssize_t read_input(
    struct sh_shell_context const *ctx,
    char **out,
    size_t *out_capacity
) {
    struct termios orig_termios;
    enable_raw_mode(&orig_termios);

    size_t buf_capacity = 64;
    size_t buf_idx = 0; // Index into the buffer.
    char *buf = malloc(sizeof(char) * buf_capacity);
    if (buf == NULL) {
        perror("read_input");
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
                free(buf);
                perror("read_input");
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

        // Handle escape sequences for navigating between history lines.
        if (c == 27 && ctx->history_count > 0) {
            char seq[3];
            seq[0] = getchar();
            seq[1] = getchar();
            seq[2] = '\0';

            // Up arrow.
            if (seq[0] == '[' && seq[1] == 'A' && history_idx > 0) {
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
                        free(buf);
                        perror("read_input");
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
            if (seq[0] == '[' && seq[1] == 'B'
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
                        free(buf);
                        perror("read_input");
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

    disable_raw_mode(&orig_termios);

    *out_capacity = buf_capacity;
    *out = buf;

    return buf_idx;
}

int main() {
    setup_signals();

    struct sh_shell_context sh_ctx;
    if (init_shell_context(&sh_ctx) != SH_INIT_SHELL_CONTEXT_SUCCESS) {
        perror("shell context initialisation");
        return EXIT_FAILURE;
    }

    // Main loop.
    bool should_exit = false;
    int exit_code = EXIT_SUCCESS;
    while (!should_exit) {
        printf("%s ", sh_ctx.prompt);

        // Read user input command.
        char *line = NULL;
        size_t line_capacity = 0;
        // `line_len` contains the number of characters in the line (including
        // the null byte), not the capacity!
        ssize_t line_len = read_input(&sh_ctx, &line, &line_capacity);

        if (line_len < 0) {
            // Consume the rest of the input in `stdin`.
            char ch;
            do {
                ch = getchar();
            } while (ch != '\n' && ch != EOF);

            continue;
        }

        add_to_history(&sh_ctx, line);

        struct sh_run_result run_result = run(&sh_ctx, line);
        if (sh_ctx.should_exit) {
            should_exit = true;
            exit_code = sh_ctx.exit_code;
        }

        // We don't free the line since it is kept in the history.
    }

    destroy_shell_context(&sh_ctx);
    return exit_code;
}

enum sh_init_shell_context_result
init_shell_context(struct sh_shell_context *ctx) {
    // Default prompt is "%".
    char *prompt = strdup("%");
    if (prompt == NULL) {
        return SH_INIT_SHELL_CONTEXT_MEMORY_ERROR;
    }

    *ctx = (struct sh_shell_context) {
        .history_capacity = 0,
        .history_count = 0,
        .history = NULL,
        .prompt = prompt,
        .should_exit = false,
        .exit_code = EXIT_SUCCESS,
    };

    return SH_INIT_SHELL_CONTEXT_SUCCESS;
}

enum sh_add_to_history_result
add_to_history(struct sh_shell_context *ctx, char *line) {
    // Grow the history buffer if needed.
    if (ctx->history_count == ctx->history_capacity) {
        size_t new_capacity = ctx->history_capacity == 0
                                  ? 4
                                  : ctx->history_capacity * 2;

        char **tmp = realloc(ctx->history, sizeof(char *) * new_capacity);
        if (tmp == NULL) {
            return SH_ADD_TO_HISTORY_MEMORY_ERROR;
        }
        ctx->history_capacity = new_capacity;
        ctx->history = tmp;
    }

    // Add the line.
    ctx->history[ctx->history_count] = line;
    ctx->history_count++;
    return SH_ADD_TO_HISTORY_SUCCESS;
}

void destroy_shell_context(struct sh_shell_context *ctx) {
    // Release memory for the history.
    for (size_t idx = 0; idx < ctx->history_count; idx++) {
        free(ctx->history[idx]);
    }
    free(ctx->history);
    ctx->history = NULL;

    // Release memory for the prompt.
    free(ctx->prompt);
}

void setup_signals() {
    // Set up SIGCHLD handler.
    struct sigaction sigact_chld;
    sigemptyset(&sigact_chld.sa_mask);
    // Prevent `getline()` from getting interrupted.
    // See `man sigaction` and search for `SA_RESTART`.
    sigact_chld.sa_flags = SA_RESTART;
    sigact_chld.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sigact_chld, NULL);

    ignore_stop_signals();
}

void ignore_stop_signals() {
    struct sigaction sigact_ign;
    sigemptyset(&sigact_ign.sa_mask);
    sigact_ign.sa_flags = 0;
    sigact_ign.sa_handler = SIG_IGN;
    for (size_t idx = 0; idx < STOP_SIGNALS_SIZE; idx++) {
        int sig = STOP_SIGNALS[idx];
        sigaction(sig, &sigact_ign, NULL);
    }
}

void reset_signal_handlers_for_stop_signals() {
    struct sigaction sigact_dfl;
    sigemptyset(&sigact_dfl.sa_mask);
    sigact_dfl.sa_flags = 0;
    sigact_dfl.sa_handler = SIG_DFL;
    for (size_t idx = 0; idx < STOP_SIGNALS_SIZE; idx++) {
        int sig = STOP_SIGNALS[idx];
        sigaction(sig, &sigact_dfl, NULL);
    }
}

void handle_sigchld(int signo) {
    // Since signals don't have a queue, it is possible for multiple `SIGCHLD`
    // signals to "combine". Hence, we need to use a loop to consume all current
    // zombie processes.
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

void enable_raw_mode(struct termios *orig_termios) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig_termios);
    raw = *orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode(struct termios *orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}
