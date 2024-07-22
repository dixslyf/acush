#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "shell.h"

enum sh_getcwd_error {
    SH_GETCWD_SUCCESS = 0,
    SH_GETCWD_MEMORY_ERROR,
    SH_GETCWD_GENERIC_ERROR
};
enum sh_getcwd_error allocating_getcwd(char **out);

bool is_builtin(char const *name) {
    return strcmp(name, "exit") == 0 || strcmp(name, "history") == 0
           || strcmp(name, "prompt") == 0 || strcmp(name, "pwd") == 0
           || strcmp(name, "cd") == 0;
}

int run_builtin(
    struct sh_shell_context *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char const *const *argv
) {
    // Handle `exit` builtin.
    if (strcmp(argv[0], "exit") == 0) {
        return run_exit(ctx, fds, argc, argv);
    }

    // Handle `prompt` builtin.
    if (strcmp(argv[0], "prompt") == 0) {
        return run_prompt(ctx, fds, argc, argv);
    }

    // Handle `cd` builtin.
    if (strcmp(argv[0], "cd") == 0) {
        return run_cd(fds, argc, argv);
    }

    // Handle `history` builtin.
    if (strcmp(argv[0], "history") == 0) {
        return run_history(ctx, fds, argc, argv);
    }

    // Handle `pwd` builtin.
    if (strcmp(argv[0], "pwd") == 0) {
        return run_pwd(fds, argc, argv);
    }

    // This function should not be called if `argv[0]` is not a builtin command!
    assert(false);
}

enum sh_exit_result run_exit(
    struct sh_shell_context *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char const *const *argv
) {
    // This function should only be called when `argv[0]` is "exit".
    assert(argc >= 1);
    assert(strcmp(argv[0], "exit") == 0);

    // More than 1 argument was given to `exit`, so we don't know how to
    // proceed.
    if (argc > 2) {
        dprintf(fds.stderr, "exit: unexpected arguments\n");
        return SH_EXIT_UNEXPECTED_ARG_COUNT;
    }

    // If there is no exit code specified, default to `EXIT_SUCCESS` (0).
    if (argc == 1) {
        ctx->should_exit = true;
        ctx->exit_code = EXIT_SUCCESS;
        return SH_EXIT_SUCCESS;
    }

    // Convert the exit code string into an integer.
    char const *exit_code_str = argv[1];
    char *endptr;
    long exit_code = strtol(exit_code_str, &endptr, 10);

    // The entire string is a valid `long` only if `*endptr` is the null byte.
    if (*endptr != '\0') {
        dprintf(fds.stderr, "exit: unexpected non-integer exit code\n");
        return SH_EXIT_NONINTEGER_EXIT_CODE;
    }

    // The exit code is outside the range of `int`.
    if (exit_code < INT_MIN || exit_code > INT_MAX) {
        dprintf(fds.stderr, "exit: out-of-range exit code\n");
        return SH_EXIT_OUT_OF_RANGE_EXIT_CODE;
    }

    ctx->should_exit = true;
    ctx->exit_code = (int) exit_code;

    return SH_EXIT_SUCCESS;
}

enum sh_history_result run_history(
    struct sh_shell_context const *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char const *const *argv
) {
    assert(argc >= 1);
    assert(strcmp(argv[0], "history") == 0);

    if (argc > 1) {
        dprintf(fds.stderr, "history: unexpected argument count\n");
        return SH_HISTORY_UNEXPECTED_ARG_COUNT;
    }

    for (size_t idx = 0; idx < ctx->history_count; idx++) {
        dprintf(fds.stdout, "%lu  %s\n", idx + 1, ctx->history[idx]);
    }

    return SH_HISTORY_SUCCESS;
}

enum sh_prompt_result run_prompt(
    struct sh_shell_context *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char const *const *argv
) {
    assert(argc >= 1);
    assert(strcmp(argv[0], "prompt") == 0);

    // Expecting exactly one argument after "prompt".
    if (argc != 2) {
        dprintf(fds.stderr, "prompt: unexpected argument count\n");
        dprintf(fds.stderr, "usage: prompt <new-prompt>\n");
        return SH_PROMPT_UNEXPECTED_ARG_COUNT;
    }

    // Allocate memory for the new prompt string.
    // We don't reuse `argv[1]` so that the prompt is independent of the
    // lifetime of the tokens — not really necessary, but easier housekeeping.
    char *tmp = strdup(argv[1]);
    if (tmp == NULL) {
        dprintf(fds.stderr, "prompt: %s\n", strerror(errno));
        return SH_PROMPT_MEMORY_ERROR;
    }

    // Free the old prompt string.
    free(ctx->prompt);
    ctx->prompt = tmp;

    return SH_PROMPT_SUCCESS;
}

enum sh_pwd_result
run_pwd(struct sh_builtin_std_fds fds, size_t argc, char const *const *argv) {
    // This function should only be called when `argv[0]` is "pwd".
    assert(argc >= 1);
    assert(strcmp(argv[0], "pwd") == 0);

    // Check for unexpected arguments.
    if (argc > 1) {
        dprintf(fds.stderr, "pwd: unexpected argument count\n");
        return SH_PWD_UNEXPECTED_ARG_COUNT;
    }

    char *cwd;
    switch (allocating_getcwd(&cwd)) {
    case SH_GETCWD_SUCCESS:
        break;
    case SH_GETCWD_MEMORY_ERROR:
        dprintf(fds.stderr, "pwd: %s\n", strerror(errno));
        return SH_PWD_MEMORY_ERROR;
    case SH_GETCWD_GENERIC_ERROR:
        dprintf(fds.stderr, "pwd: %s\n", strerror(errno));
        return SH_PWD_GENERIC_ERROR;
    }

    dprintf(fds.stdout, "%s\n", cwd);
    free(cwd);
    return SH_PWD_SUCCESS;
}

enum sh_cd_result
run_cd(struct sh_builtin_std_fds fds, size_t argc, char const *const *argv) {
    // This function should only be called when `argv[0]` is "pwd".
    assert(argc >= 1);
    assert(strcmp(argv[0], "cd") == 0);

    // Check for unexpected arguments.
    if (argc > 2) {
        dprintf(fds.stderr, "cd: unexpected argument count\n");
        return SH_CD_UNEXPECTED_ARG_COUNT;
    }

    char *oldpwd = NULL;
    switch (allocating_getcwd(&oldpwd)) {
    case SH_GETCWD_SUCCESS:
        break;
    case SH_GETCWD_MEMORY_ERROR:
        dprintf(fds.stderr, "cd: %s\n", strerror(errno));
        return SH_CD_MEMORY_ERROR;
    case SH_GETCWD_GENERIC_ERROR:
        dprintf(fds.stderr, "cd: %s\n", strerror(errno));
        return SH_CD_GENERIC_ERROR;
    }

    // Default to the user's home directory if no argument is given.
    // It seems like `bash` also uses the `HOME` environment variable to get the
    // home directory.
    char const *dir;
    bool should_pwd = false;
    if (argc == 2 && strcmp(argv[1], "-") == 0) {
        dir = getenv("OLDPWD");
        if (dir == NULL) {
            free(oldpwd);
            dprintf(fds.stderr, "cd: OLDPWD is not set\n");
            return SH_CD_OLDPWD_NOT_SET;
        }
        should_pwd = true;
    } else if (argc == 2) {
        dir = argv[1];
    } else {
        dir = getenv("HOME");
        if (dir == NULL) {
            free(oldpwd);
            dprintf(fds.stderr, "cd: HOME is not set\n");
            return SH_CD_HOME_NOT_SET;
        }
    }

    if (chdir(dir) < 0) {
        dprintf(fds.stderr, "cd: %s\n", strerror(errno));
        free(oldpwd);
        return SH_CD_GENERIC_ERROR;
    }

    if (should_pwd) {
        char const *argv[1] = {"pwd"};
        if (run_pwd(fds, 1, argv)) {
            free(oldpwd);
            return SH_CD_GENERIC_ERROR;
        }
    }

    // We cannot simply use `dir` as it may be a relative path.
    // The `PWD` environment variable should be set to a full path.
    char *pwd = NULL;
    switch (allocating_getcwd(&pwd)) {
    case SH_GETCWD_SUCCESS:
        break;
    case SH_GETCWD_MEMORY_ERROR:
        dprintf(fds.stderr, "cd: %s\n", strerror(errno));
        return SH_CD_MEMORY_ERROR;
    case SH_GETCWD_GENERIC_ERROR:
        dprintf(fds.stderr, "cd: %s\n", strerror(errno));
        return SH_CD_GENERIC_ERROR;
    }

    if (setenv("OLDPWD", oldpwd, 1) < 0 || setenv("PWD", pwd, 1) < 0) {
        perror("cd");
        free(oldpwd);
        free(pwd);
        return SH_CD_GENERIC_ERROR;
    };

    free(oldpwd);
    free(pwd);
    return SH_CD_SUCCESS;
}

enum sh_getcwd_error allocating_getcwd(char **out) {
    // Initial buffer size for the current working directory.
    // `PATH_MAX` from `<limits.h` is, unfortunately, not an accurate value for
    // the true max path size. See
    // https://stackoverflow.com/questions/9449241/where-is-path-max-defined-in-linux.
    size_t size = 128;
    char *curdir = malloc(sizeof(char) * size);
    if (curdir == NULL) {
        return SH_GETCWD_MEMORY_ERROR;
    }

    // Get the current working directory.
    while (getcwd(curdir, size) == NULL) {
        if (errno == ERANGE) {
            // Buffer was too small, so increase its size.
            size *= 2;
            char *new_cwd = realloc(curdir, sizeof(char) * size);
            if (new_cwd == NULL) {
                free(curdir);
                return SH_GETCWD_MEMORY_ERROR;
            }
            curdir = new_cwd;
        } else {
            // Some other error occurred.
            free(curdir);
            return SH_GETCWD_GENERIC_ERROR;
        }
    }

    *out = curdir;
    return SH_GETCWD_SUCCESS;
}
