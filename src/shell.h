/**
 * @file shell.h
 *
 * Declarations for the shell context and associated functions.
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stdlib.h>

/** Keeps track of various stateful information about the current shell. */
struct sh_shell_context {
    size_t history_capacity; /**< Capacity of the history array. */
    size_t history_count;    /**< Current number of entries in the history. */
    char **history;          /**< Array of command history strings. */

    char *prompt; /**< The current shell prompt. */

    bool should_exit; /**< Indicates if the shell should exit. This is set by
                         the `exit` builtin. */
    int exit_code;    /**<
                       * Exit code for the shell if the shell should exit..
                       * This is set by the `exit` builtin.
                       */
};

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

#endif /* SHELL_H */
