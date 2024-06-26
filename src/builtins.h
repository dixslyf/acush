#ifndef BUILTINS_H
#define BUILTINS_H

#include "shell.h"
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

enum sh_history_result {
    SH_HISTORY_SUCCESS = 0,
    SH_HISTORY_UNEXPECTED_ARG_COUNT,
};

enum sh_history_result
run_history(struct sh_shell_context const *ctx, size_t argc, char *argv[]);

enum sh_prompt_result {
    SH_PROMPT_SUCCESS = 0,
    SH_PROMPT_UNEXPECTED_ARG_COUNT,
    SH_PROMPT_MEMORY_ERROR,
};

enum sh_prompt_result
run_prompt(struct sh_shell_context *ctx, size_t argc, char *argv[]);

enum sh_pwd_result {
    SH_PWD_SUCCESS = 0,
    SH_PWD_UNEXPECTED_ARG_COUNT,
    SH_PWD_MEMORY_ERROR,
    SH_PWD_GENERIC_ERROR,
};

enum sh_pwd_result run_pwd(size_t argc, char *argv[]);

enum sh_cd_result {
    SH_CD_SUCCESS = 0,
    SH_CD_UNEXPECTED_ARG_COUNT,
    SH_CD_GENERIC_ERROR,
};

enum sh_cd_result run_cd(size_t argc, char *argv[]);

#endif
