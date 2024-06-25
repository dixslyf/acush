#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdlib.h>

enum sh_exit_result_type {
    SH_EXIT_SUCCESS,
    SH_EXIT_NONINTEGER_EXIT_CODE,
    SH_EXIT_UNEXPECTED_ARG_COUNT,
    SH_EXIT_OUT_OF_RANGE_EXIT_CODE,
};

struct sh_exit_result {
    enum sh_exit_result_type type;
    union {
        int exit_code;
    };
};

struct sh_exit_result run_exit(size_t argc, char *argv[]);

#endif
