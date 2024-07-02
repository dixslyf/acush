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

#endif /* SHELL_H */
