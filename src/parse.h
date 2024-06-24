#ifndef PARSE_H
#define PARSE_H

#include <stdlib.h>

#include "lex.h"

struct sh_ast_simple_cmd {
    size_t argc;
    char **argv;
};

struct sh_ast_cmd {
    struct sh_ast_simple_cmd simple_cmd;
    enum {
        SH_REDIRECT_NONE,
        SH_REDIRECT_STDOUT,
        SH_REDIRECT_STDIN,
        SH_REDIRECT_STDERR
    } redirect_type;
    char *redirect_file;
};

struct sh_ast_job {
    struct sh_ast_cmd *piped_cmds;
    size_t cmd_count;
};

struct sh_job_desc {
    enum { SH_JOB_FG, SH_JOB_BG } type;
    struct sh_ast_job job;
};

struct sh_ast_cmd_line {
    enum { SH_COMMAND_REPEAT, SH_COMMAND_JOBS } type;
    union {
        char *repeat;
        struct {
            size_t job_count;
            struct sh_job_desc *job_descs;
        };
    };
};

struct sh_ast_root {
    enum { SH_ROOT_EMPTY, SH_ROOT_NONEMPTY } emptiness;
    union { // A union is technically not needed, but it's used here to
            // semantically indicate that `cmd_line` can be uninitialised.
        struct sh_ast_cmd_line cmd_line;
    };
};

enum sh_parse_result {
    /** Indicates a successful parse. */
    SH_PARSE_SUCCESS,

    SH_PARSE_MEMORY_ERROR,

    SH_PARSE_UNEXPECTED_TOKENS,

    SH_PARSE_COMMAND_LINE_FAIL,

    SH_PARSE_JOB_FAIL,

    SH_PARSE_COMMAND_FAIL,

    SH_PARSE_SIMPLE_COMMAND_FAIL,

    SH_PARSE_UNEXPECTED_END,
};

enum sh_parse_result
parse(struct sh_token tokens[], size_t token_count, struct sh_ast_root *out);

void destroy_ast(struct sh_ast_root *ast_root);

void display_ast(struct sh_ast_root *ast_root);

#endif
