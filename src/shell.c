#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "run.h"
#include "shell.h"

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
        size_t line_size = 0; // Will contain the size of the `line`
                              // buffer, not the number of characters!
        ssize_t line_len = getline(&line, &line_size, stdin);
        // `line_len` contains the number of characters (including the
        // delimiter), not the buffer size!

        if (line_len < 0) {
            // According to `getline`'s documentation,
            // `line` must be freed even if `getline` fails.
            free(line);

            perror("getline");

            // Consume the rest of the input in `stdin`.
            char ch;
            do {
                ch = getchar();
            } while (ch != '\n' && ch != EOF);

            continue;
        }

        // Strip trailing newline.
        if (line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
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

    // Ignore SIGINT (Ctrl+C), SIGQUIT (Ctrl+\) and SIGTSTP (Ctrl+Z).
    static size_t const SIGNALS_IGNORE_SIZE = 3;
    static int const SIGNALS_IGNORE[] = {SIGINT, SIGQUIT, SIGTSTP};
    struct sigaction sigact_ign;
    sigemptyset(&sigact_ign.sa_mask);
    sigact_ign.sa_flags = 0;
    sigact_ign.sa_handler = SIG_IGN;
    for (size_t idx = 0; idx < SIGNALS_IGNORE_SIZE; idx++) {
        int sig = SIGNALS_IGNORE[idx];
        sigaction(sig, &sigact_ign, NULL);
    }
}

void handle_sigchld(int signo) {
    // Since signals don't have a queue, it is possible for multiple `SIGCHLD`
    // signals to "combine". Hence, we need to use a loop to consume all current
    // zombie processes.
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}
