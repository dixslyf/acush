#ifndef RUN_H
#define RUN_H

#include <stdbool.h>

#include "parse.h"

struct sh_run_result {
    bool should_exit;
    int exit_code;

    size_t error_count;
    struct sh_run_error *errors;
};

struct sh_run_error {
    // TODO:
};

struct sh_run_result run(struct sh_ast_root const *root);

#endif
