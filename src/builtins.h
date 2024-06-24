#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdlib.h>

typedef enum sh_exit_result_type sh_exit_result_type;
enum sh_exit_result_type {
    SH_EXIT_SUCCESS,
    SH_EXIT_NONINTEGER_EXIT_CODE,
    SH_EXIT_UNEXPECTED_ARG_COUNT,
    SH_EXIT_OUT_OF_RANGE_EXIT_CODE,
};

typedef struct sh_exit_result sh_exit_result;
struct sh_exit_result {
    sh_exit_result_type type;
    union {
        int exit_code;
    };
};

sh_exit_result run_exit(size_t argc, char *argv[]);

#endif
