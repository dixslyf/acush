#ifndef PARSE_H
#define PARSE_H

#include <stdlib.h>

#include "lex.h"

// Forward declaration.
struct sh_ast;

enum sh_ast_type {
    SH_AST_ROOT,
    SH_AST_COMMAND_LINE,
    SH_AST_JOB,
    SH_AST_COMMAND,
    SH_AST_SIMPLE_COMMAND,
};

enum sh_job_type { SH_JOB_FG, SH_JOB_BG };

struct sh_job {
    enum sh_job_type job_type;
    struct sh_ast *ast_node;
};

enum sh_redirect_type {
    SH_REDIRECT_NONE,
    SH_REDIRECT_STDOUT,
    SH_REDIRECT_STDIN,
    SH_REDIRECT_STDERR
};

union sh_ast_data {
    struct {
        struct sh_ast *command_line;
    } root;

    struct {
        enum { SH_COMMAND_REPEAT, SH_COMMAND_JOBS } type;
        union {
            struct {
                struct sh_job *jobs;
                size_t job_count;
            };
            char *repeat;
        };
    } command_line;

    struct {
        struct sh_ast **piped_cmds;
        size_t cmd_count;
    } job;

    struct {
        struct sh_ast *simple_command;
        enum sh_redirect_type redirect_type;
        char *redirect_file;
    } command;

    struct {
        size_t argc;
        char **argv;
    } simple_command;
};

struct sh_ast {
    enum sh_ast_type type;
    union sh_ast_data data;
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
parse(struct sh_token tokens[], size_t token_count, struct sh_ast **ast_out);

void destroy_ast_node(struct sh_ast *ast);

void display_ast(struct sh_ast *ast);

#endif
