/**
 * @file builtins.h
 * Declarations for shell built-in commands.
 */

#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>
#include <stdio.h>

#include "shell.h"

/**
 * Contains file descriptors for the standard streams.
 * Each built-in command function uses these file descriptors for their output.
 */
struct sh_builtin_std_fds {
    int stdin;  /**< Standard input file descriptor */
    int stdout; /**< Standard output file descriptor */
    int stderr; /**< Standard error file descriptor */
};

/**
 * Checks if a given name is a built-in command.
 *
 * Callers should pass in `argv[0]` as the name.
 *
 * @param name the name of the command
 * @return `true if the command is a built-in, otherwise `false`
 */
bool is_builtin(char const *name);

/**
 * Runs a built-in command.
 *
 * `argv[0]` must represent a valid built-in command! That is,
 * `is_builtin(argv[0])` must evaluate to `true`!
 *
 * @param ctx a pointer to the shell context
 * @param fds standard streams' file descriptors for the built-in command's
 * output
 * @param argc the number of arguments
 * @param argv the argument vector
 *
 * @return status code of the command execution
 */
int run_builtin(
    struct sh_shell_context *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char const *const *argv
);

/** Represents the possible results for the `exit` built-in command. */
enum sh_exit_result {
    SH_EXIT_SUCCESS = 0,            /**< Successful exit. */
    SH_EXIT_NONINTEGER_EXIT_CODE,   /**< Exit code is not an integer. */
    SH_EXIT_UNEXPECTED_ARG_COUNT,   /**< Unexpected number of arguments. */
    SH_EXIT_OUT_OF_RANGE_EXIT_CODE, /**< Exit code is out of range. */
};

/**
 * Runs the `exit` built-in command.
 *
 * @param ctx a pointer to the shell context
 * @param fds standard streams' file descriptors for the built-in command's
 * output
 * @param argc the number of arguments
 * @param argv the argument vector
 *
 * @return the result of the exit command
 */
enum sh_exit_result run_exit(
    struct sh_shell_context *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char const *const *argv
);

/** Represents the possible results for the `history` built-in command. */
enum sh_history_result {
    SH_HISTORY_SUCCESS = 0,          /**< Successful execution */
    SH_HISTORY_UNEXPECTED_ARG_COUNT, /**< Unexpected number of arguments */
};

/**
 * Runs the `history` built-in command.
 *
 * @param ctx a pointer to the shell context
 * @param fds standard streams' file descriptors for the built-in command's
 * output
 * @param argc the number of arguments
 * @param argv the argument vector
 *
 * @return the result of the history command
 */
enum sh_history_result run_history(
    struct sh_shell_context const *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char const *const *argv
);

/** Represents the possible results for the `prompt` built-in command. */
enum sh_prompt_result {
    SH_PROMPT_SUCCESS = 0,          /**< Successful execution */
    SH_PROMPT_UNEXPECTED_ARG_COUNT, /**< Unexpected number of arguments */
    SH_PROMPT_MEMORY_ERROR,         /**< Memory error */
};

/**
 * Runs the `prompt` built-in command.
 *
 * @param ctx a pointer to the shell context
 * @param fds standard streams' file descriptors for the built-in command's
 * output
 * @param argc the number of arguments
 * @param argv the argument vector
 *
 * @return the result of the prompt command
 */
enum sh_prompt_result run_prompt(
    struct sh_shell_context *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char const *const *argv
);

/** Represents the possible results for the `pwd` built-in command. */
enum sh_pwd_result {
    SH_PWD_SUCCESS = 0,          /**< Successful execution */
    SH_PWD_UNEXPECTED_ARG_COUNT, /**< Unexpected number of arguments */
    SH_PWD_MEMORY_ERROR,         /**< Memory error */
    SH_PWD_GENERIC_ERROR,        /**< Generic error */
};

/**
 * Runs the `pwd` built-in command.
 *
 * @param fds standard streams' file descriptors for the built-in command's
 * output
 * @param argc the number of arguments
 * @param argv the argument vector
 *
 * @return the result of the pwd command
 */
enum sh_pwd_result
run_pwd(struct sh_builtin_std_fds fds, size_t argc, char const *const *argv);

/** Represents the possible results for the `cd` built-in command. */
enum sh_cd_result {
    SH_CD_SUCCESS = 0,          /**< Successful execution */
    SH_CD_UNEXPECTED_ARG_COUNT, /**< Unexpected number of arguments */
    SH_CD_GENERIC_ERROR,        /**< Generic error */
};

/**
 * Runs the `cd` built-in command.
 *
 * @param fds standard streams' file descriptors for the built-in command's
 * output
 * @param argc the number of arguments
 * @param argv the argument vector
 *
 * @return the result of the cd command
 */
enum sh_cd_result
run_cd(struct sh_builtin_std_fds fds, size_t argc, char const *const *argv);

#endif /* BUILTINS_H */
