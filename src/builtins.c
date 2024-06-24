#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"

sh_exit_result run_exit(size_t argc, char *argv[]) {
    // This function should only be called when `argv[0]` is "exit".
    assert(argc >= 1);
    assert(strcmp(argv[0], "exit") == 0);

    sh_exit_result result;

    // More than 1 argument was given to `exit`, so we don't know how to
    // proceed.
    if (argc > 2) {
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
        result.type = SH_EXIT_NONINTEGER_EXIT_CODE;
        return result;
    }

    // The exit code is outside the range of `int`.
    if (exit_code < INT_MIN || exit_code > INT_MAX) {
        result.type = SH_EXIT_OUT_OF_RANGE_EXIT_CODE;
        return result;
    }

    result.type = SH_EXIT_SUCCESS;
    result.exit_code = (int) exit_code;

    return result;
}
