#ifndef SHELL_H
#define SHELL_H

#include <stdlib.h>

struct sh_shell_context {
    size_t history_capacity;
    size_t history_count;
    char **history;
    char *prompt; /**< The current shell prompt. */
};

enum sh_init_shell_context_result {
    SH_INIT_SHELL_CONTEXT_SUCCESS,
    SH_INIT_SHELL_CONTEXT_MEMORY_ERROR,
};

enum sh_init_shell_context_result
init_shell_context(struct sh_shell_context *ctx);

enum sh_add_to_history_result {
    SH_ADD_TO_HISTORY_SUCCESS,
    SH_ADD_TO_HISTORY_MEMORY_ERROR,
};

enum sh_add_to_history_result
add_to_history(struct sh_shell_context *ctx, char *line);

void destroy_shell_context(struct sh_shell_context *ctx);

/** Sets up signal handling. */
void setup_signals();

/**
 * Handler for `SIGCHLD` signal.
 *
 * This handler consumes background processes with `waitpid()` to make sure they
 * do not become zombie processes.
 */
void handle_sigchld(int signo);

#endif
