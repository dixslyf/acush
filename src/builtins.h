#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>
#include <stdlib.h>

#include "shell.h"

struct sh_builtin_std_fds {
    int stdin;
    int stdout;
    int stderr;
};

bool is_builtin(char *name);

int run_builtin(
    struct sh_shell_context *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char *argv[]
);

enum sh_exit_result {
    SH_EXIT_SUCCESS,
    SH_EXIT_NONINTEGER_EXIT_CODE,
    SH_EXIT_UNEXPECTED_ARG_COUNT,
    SH_EXIT_OUT_OF_RANGE_EXIT_CODE,
};

enum sh_exit_result run_exit(
    struct sh_shell_context *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char *argv[]
);

enum sh_history_result {
    SH_HISTORY_SUCCESS = 0,
    SH_HISTORY_UNEXPECTED_ARG_COUNT,
};

enum sh_history_result run_history(
    struct sh_shell_context const *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char *argv[]
);

enum sh_prompt_result {
    SH_PROMPT_SUCCESS = 0,
    SH_PROMPT_UNEXPECTED_ARG_COUNT,
    SH_PROMPT_MEMORY_ERROR,
};

enum sh_prompt_result run_prompt(
    struct sh_shell_context *ctx,
    struct sh_builtin_std_fds fds,
    size_t argc,
    char *argv[]
);

enum sh_pwd_result {
    SH_PWD_SUCCESS = 0,
    SH_PWD_UNEXPECTED_ARG_COUNT,
    SH_PWD_MEMORY_ERROR,
    SH_PWD_GENERIC_ERROR,
};

enum sh_pwd_result
run_pwd(struct sh_builtin_std_fds fds, size_t argc, char *argv[]);

enum sh_cd_result {
    SH_CD_SUCCESS = 0,
    SH_CD_UNEXPECTED_ARG_COUNT,
    SH_CD_GENERIC_ERROR,
};

enum sh_cd_result
run_cd(struct sh_builtin_std_fds fds, size_t argc, char *argv[]);

#endif
