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

struct sh_command_history {
    size_t count;
    char *commands[MAX_HISTORY];
};

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
    struct sh_command_history history_memory;
};

/** Sets up signal handlers for SIGINT (Ctrl+C), SIGQUIT (Ctrl+\) and SIGTSTP
 * (Ctrl+Z) to ignore them. */
void ignore_stop_signals();

/** Resets the signal handlers for SIGINT (Ctrl+C), SIGQUIT (Ctrl+\) and SIGTSTP
 * (Ctrl+Z) to their defaults. */
void reset_signal_handlers_for_stop_signals();

void init_shell_context_for_history(struct sh_shell_context *ctx);
void add_command_to_history(struct sh_shell_context *ctx, char const *command);
char *get_command_by_number(struct sh_shell_context *ctx, size_t number);
char *get_command_by_prefix(struct sh_shell_context *ctx, char const *prefix);

#endif /* SHELL_H */
