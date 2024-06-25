#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        fprintf(stderr, "history: unexpected arguments\n");
        return SH_HISTORY_UNEXPECTED_ARG_COUNT;
    }

    for (size_t idx = 0; idx < ctx->history_count; idx++) {
        printf("%lu  %s\n", idx + 1, ctx->history[idx]);
    }

    return SH_HISTORY_SUCCESS;
}
