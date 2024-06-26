#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"

struct sh_exit_result run_exit(size_t argc, char *argv[]) {
    // This function should only be called when `argv[0]` is "exit".
    assert(argc >= 1);
    assert(strcmp(argv[0], "exit") == 0);

    struct sh_exit_result result;

    // More than 1 argument was given to `exit`, so we don't know how to
    // proceed.
    if (argc > 2) {
        fprintf(stderr, "exit: unexpected arguments\n");
        result.type = SH_EXIT_UNEXPECTED_ARG_COUNT;
        return result;
    }

    // If there is no exit code specified, default to `EXIT_SUCCESS` (0).
    if (argc == 1) {
        result.type = SH_EXIT_SUCCESS;
        result.exit_code = EXIT_SUCCESS;
        return result;
    }

    // Convert the exit code string into an integer.
    char *exit_code_str = argv[1];
    char *endptr;
    long exit_code = strtol(exit_code_str, &endptr, 10);

    // The entire string is a valid `long` only if `*endptr` is the null byte.
    if (*endptr != '\0') {
        fprintf(stderr, "exit: unexpected non-integer exit code\n");
        result.type = SH_EXIT_NONINTEGER_EXIT_CODE;
        return result;
    }

    // The exit code is outside the range of `int`.
    if (exit_code < INT_MIN || exit_code > INT_MAX) {
        fprintf(stderr, "exit: out-of-range exit code\n");
        result.type = SH_EXIT_OUT_OF_RANGE_EXIT_CODE;
        return result;
    }

    result.type = SH_EXIT_SUCCESS;
    result.exit_code = (int) exit_code;

    return result;
}

enum sh_history_result
run_history(struct sh_shell_context const *ctx, size_t argc, char *argv[]) {
    assert(argc >= 1);
    assert(strcmp(argv[0], "history") == 0);

    if (argc > 1) {
        fprintf(stderr, "history: unexpected argument count\n");
        return SH_HISTORY_UNEXPECTED_ARG_COUNT;
    }

    for (size_t idx = 0; idx < ctx->history_count; idx++) {
        printf("%lu  %s\n", idx + 1, ctx->history[idx]);
    }

    return SH_HISTORY_SUCCESS;
}

enum sh_prompt_result
run_prompt(struct sh_shell_context *ctx, size_t argc, char *argv[]) {
    assert(argc >= 1);
    assert(strcmp(argv[0], "prompt") == 0);

    // Expecting exactly one argument after "prompt".
    if (argc != 2) {
        fprintf(stderr, "prompt: unexpected argument count\n");
        fprintf(stderr, "usage: prompt <new-prompt>\n");
        return SH_PROMPT_UNEXPECTED_ARG_COUNT;
    }

    // Allocate memory for the new prompt string.
    // We don't reuse `argv[1]` so that the prompt is independent of the
    // lifetime of the tokens — not really necessary, but easier housekeeping.
    char *tmp = strdup(argv[1]);
    if (tmp == NULL) {
        perror("prompt");
        return SH_PROMPT_MEMORY_ERROR;
    }

    // Free the old prompt string.
    free(ctx->prompt);
    ctx->prompt = tmp;

    return SH_PROMPT_SUCCESS;
}

enum sh_pwd_result run_pwd(size_t argc, char *argv[]) {
    // This function should only be called when `argv[0]` is "pwd".
    assert(argc >= 1);
    assert(strcmp(argv[0], "pwd") == 0);

    // Check for unexpected arguments.
    if (argc > 1) {
        fprintf(stderr, "pwd: unexpected argument count\n");
        return SH_PWD_UNEXPECTED_ARG_COUNT;
    }

    // Initial buffer size for the current working directory.
    // `PATH_MAX` from `<limits.h` is, unfortunately, not an accurate value for
    // the true max path size. See
    // https://stackoverflow.com/questions/9449241/where-is-path-max-defined-in-linux.
    size_t size = 128;
    char *cwd = malloc(sizeof(char) * size);
    if (cwd == NULL) {
        perror("pwd");
        return SH_PWD_MEMORY_ERROR;
    }

    // Get the current working directory.
    while (getcwd(cwd, size) == NULL) {
        if (errno == ERANGE) {
            // Buffer was too small, so increase its size.
            size *= 2;
            char *new_cwd = realloc(cwd, sizeof(char) * size);
            if (new_cwd == NULL) {
                free(cwd);
                perror("pwd");
                return SH_PWD_MEMORY_ERROR;
            }
            cwd = new_cwd;
        } else {
            // Some other error occurred.
            free(cwd);
            perror("pwd");
            return SH_PWD_GENERIC_ERROR;
        }
    }

    printf("%s\n", cwd);
    free(cwd);
    return SH_PWD_SUCCESS;
}

enum sh_cd_result run_cd(size_t argc, char *argv[]) {
    // This function should only be called when `argv[0]` is "pwd".
    assert(argc >= 1);
    assert(strcmp(argv[0], "cd") == 0);

    // Check for unexpected arguments.
    if (argc > 2) {
        fprintf(stderr, "cd: unexpected argument count\n");
        return SH_CD_UNEXPECTED_ARG_COUNT;
    }

    // Default to the user's home directory if no argument is given.
    // It seems like `bash` also uses the `HOME` environment variable to get the
    // home directory.
    char *dir = argc == 2 ? argv[1] : getenv("HOME");
    if (chdir(dir) < 0) {
        perror("cd");
        return SH_CD_GENERIC_ERROR;
    }

    return SH_CD_SUCCESS;
}
