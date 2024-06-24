#ifndef RUN_H
#define RUN_H

#include <stdbool.h>

#include "parse.h"

typedef struct sh_run_result sh_run_result;
typedef struct sh_run_error sh_run_error;

struct sh_run_result {
    bool should_exit;
    int exit_code;

    size_t error_count;
    sh_run_error *errors;
};

struct sh_run_error {
    // TODO:
};

sh_run_result run(struct sh_ast_root const *root);

#endif
