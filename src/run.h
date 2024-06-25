#ifndef RUN_H
#define RUN_H

#include <stdbool.h>

#include "parse.h"
#include "shell.h"

struct sh_run_result {
    bool should_exit;
    int exit_code;

    size_t error_count;
    struct sh_run_error *errors;
};

struct sh_run_error {
    // TODO:
};

struct sh_run_result
run(struct sh_shell_context *ctx, struct sh_ast_root const *root);

#endif
