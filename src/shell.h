/**
 * @file shell.h
 *
 * Declarations for the shell context and associated functions.
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stdlib.h>

#define MAX_HISTORY 100

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
add_line_to_history(struct sh_shell_context *ctx, char const *line);

char *get_command_by_index(struct sh_shell_context *ctx, size_t idx);

char *get_command_by_prefix(struct sh_shell_context *ctx, char const *prefix);

/** Sets up signal handlers for SIGINT (Ctrl+C), SIGQUIT (Ctrl+\) and SIGTSTP
 * (Ctrl+Z) to ignore them. */
void ignore_stop_signals();

/** Resets the signal handlers for SIGINT (Ctrl+C), SIGQUIT (Ctrl+\) and SIGTSTP
 * (Ctrl+Z) to their defaults. */
void reset_signal_handlers_for_stop_signals();

#endif /* SHELL_H */
