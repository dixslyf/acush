#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>

#include "lex.h"

/** Represents a simple shell command. */
struct sh_ast_simple_cmd {
    size_t argc; /**< Number of arguments. */
    char **argv; /**< Argument strings */
};

/** Represents a shell command with redirection. */
struct sh_ast_cmd {
    /** The simple command to (potentially) redirect. */
    struct sh_ast_simple_cmd simple_cmd;

    /** The type of redirection. */
    enum {
        SH_REDIRECT_NONE,   /**< No redirection. */
        SH_REDIRECT_STDOUT, /**< Redirect stdout (`>`). */
        SH_REDIRECT_STDIN,  /**< Redirect stdin (`<`). */
        SH_REDIRECT_STDERR  /**< Redirect stderr (`2>`). */
    } redirect_type;

    /** The file path to redirect to. */
    char *redirect_file;
};

/** Represents a shell job containing piped commands. */
struct sh_ast_job {
    /** Number of commands. */
    size_t cmd_count;

    /** Commands to pipe. The first command is piped to the second, the second
     * to the third, etc. */
    struct sh_ast_cmd *piped_cmds;
};

/** Indicates whether the job should run in the foreground (`;` or omitted) or
 * background (`&`). */
enum sh_job_type { SH_JOB_FG, SH_JOB_BG };

/** Describes a job to be run in the foreground or background. */
struct sh_job_desc {
    /** The type of the job. */
    enum sh_job_type type;

    /** The described job. */
    struct sh_ast_job job;
};

/** Represents a command line input. */
struct sh_ast_cmd_line {
    /** Indicates whether the command line is for repeating a command from the
     * history or for executing a sequence of jobs.  */
    enum { SH_COMMAND_REPEAT, SH_COMMAND_JOBS } type;

    /** Union with two members. The `repeat` member is set when the command line
     * type is `SH_COMMAND_REPEAT`. The struct member (containing `job_count`
     * and `job_descs`) is set when the type is `SH_COMMAND_JOBS`. */
    union {
        /** A string representing the start or index of the command to search
         * for and repeat. */
        char *repeat;

        struct {
            size_t job_count; /**< Number of jobs in the command line. */
            struct sh_job_desc *job_descs; /**< Job descriptions to execute. */
        };
    };
};

/**
 * Represents the root of the abstract syntax tree (AST).
 *
 * The root will either be empty (representing an empty input) or nonempty, in
 * which case it will contain a child representing the command line.
 */
struct sh_ast_root {
    /** Indicates if the root is empty. */
    enum { SH_ROOT_EMPTY, SH_ROOT_NONEMPTY } emptiness;

    // A union is technically not needed, but it's used here to
    // semantically indicate that `cmd_line` can be uninitialised (when the root
    // is empty).
    union {
        /** The command line node. This will be set when `emptiness` is
         * `SH_ROOT_NONEMPTY`. */
        struct sh_ast_cmd_line cmd_line;
    };
};

/** Represents the result of parsing. */
enum sh_parse_result {
    SH_PARSE_SUCCESS,             /**< Successful parse. */
    SH_PARSE_MEMORY_ERROR,        /**< Memory error during parsing. */
    SH_PARSE_UNEXPECTED_TOKENS,   /**< Unexpected tokens encountered. */
    SH_PARSE_COMMAND_LINE_FAIL,   /**< Command line parsing failure. */
    SH_PARSE_JOB_FAIL,            /**< Job parsing failure. */
    SH_PARSE_COMMAND_FAIL,        /**< Command parsing failure. */
    SH_PARSE_SIMPLE_COMMAND_FAIL, /**< Simple command parsing failure. */
    SH_PARSE_UNEXPECTED_END,      /**< Unexpected end of tokens. */
};

/**
 * Parses a sequence of tokens into an abstract syntax tree (AST).
 *
 * @param tokens sequence of tokens to parse
 * @param token_count number of tokens
 * @param out pointer to the AST root to set
 * @return the result of the parsing operation
 */
enum sh_parse_result
parse(struct sh_token tokens[], size_t token_count, struct sh_ast_root *out);

/**
 * Destroys the AST and frees associated memory.
 *
 * @param ast_root pointer to the AST root to destroy
 */
void destroy_ast(struct sh_ast_root *ast_root);

/**
 * Displays the AST for debugging purposes.
 *
 * @param ast_root pointer to the root of the AST to display
 */
void display_ast(FILE *stream, struct sh_ast_root *ast_root);

#endif
