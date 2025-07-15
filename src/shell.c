#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "input.h"
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
        fflush(stdout);

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

        run(&sh_ctx, line);
        if (sh_ctx.should_exit) {
            should_exit = true;
            exit_code = sh_ctx.exit_code;
        }

        free(line);
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
add_line_to_history(struct sh_shell_context *ctx, char const *line) {
    char *line_copy = strdup(line);
    if (line_copy == NULL) {
        return SH_ADD_TO_HISTORY_MEMORY_ERROR;
    }

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
    ctx->history[ctx->history_count] = line_copy;
    ctx->history_count++;
    return SH_ADD_TO_HISTORY_SUCCESS;
}

char *get_command_by_index(struct sh_shell_context *ctx, size_t idx) {
    if (idx >= ctx->history_count) {
        return NULL;
    }
    return ctx->history[idx];
}

char *get_command_by_prefix(struct sh_shell_context *ctx, char const *prefix) {
    if (ctx->history_count == 0) {
        return NULL;
    }

    char *match = NULL;
    size_t prefix_len = strlen(prefix);
    for (size_t idx = ctx->history_count; idx >= 1; idx--) {
        size_t history_cmd_len = strlen(ctx->history[idx - 1]);
        if (prefix_len <= history_cmd_len
            && strncmp(ctx->history[idx - 1], prefix, prefix_len) == 0)
        {
            match = ctx->history[idx - 1];
            break;
        }
    }
    return match;
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
    // Prevent slow system calls from getting interrupted.
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
