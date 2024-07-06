#include "parse.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "lex.h"

/** Contains context information for parsing. */
struct sh_parse_context {
    struct sh_token *tokens; /**< Sequence of tokens. */
    size_t token_count;      /**< Number of tokens in the sequence. */
    size_t token_idx;        /**< Current index in the token sequence. */
};

/**
 * Destroys a simple command AST node and frees associated memory.
 *
 * @param simple_cmd pointer to the simple command node to destroy
 */
void destroy_simple_cmd(struct sh_ast_simple_cmd *simple_cmd);

/**
 * Destroys a command AST node and frees associated memory.
 *
 * @param cmd pointer to the command node to destroy
 */
void destroy_cmd(struct sh_ast_cmd *cmd);

/**
 * Destroys a job AST node and frees associated memory.
 *
 * @param job pointer to the job node to destroy
 */
void destroy_job(struct sh_ast_job *job);

/**
 * Destroys a command line AST node and frees associated memory.
 *
 * @param cmd_line pointer to the command line node to destroy
 */
void destroy_cmd_line(struct sh_ast_cmd_line *cmd_line);

/**
 * Parses a command line AST node from the given token context.
 *
 * @param ctx pointer to the context
 * @param out pointer to the command line AST node to write to
 * @return the result of the parsing operation
 */
enum sh_parse_result
parse_cmd_line(struct sh_parse_context *ctx, struct sh_ast_cmd_line *out);

/**
 * Parses a job AST node from the given token context.
 *
 * @param ctx pointer to the context
 * @param out pointer to the job AST node to write to
 * @return the result of the parsing operation
 */
enum sh_parse_result
parse_job(struct sh_parse_context *ctx, struct sh_ast_job *out);

/**
 * Parses a command AST node from the given token context.
 *
 * @param ctx pointer to the context
 * @param out pointer to the command AST node to write to
 * @return the result of the parsing operation
 */
enum sh_parse_result
parse_cmd(struct sh_parse_context *ctx, struct sh_ast_cmd *out);

/**
 * Parses a simple command AST node from the given token context.
 *
 * @param ctx pointer to the context
 * @param out pointer to the simple command AST node to write to
 * @return the result of the parsing operation
 */
enum sh_parse_result
parse_simple_cmd(struct sh_parse_context *ctx, struct sh_ast_simple_cmd *out);

enum sh_parse_result
parse(struct sh_token tokens[], size_t token_count, struct sh_ast_root *out) {
    struct sh_parse_context ctx =
        {.tokens = tokens, .token_count = token_count, .token_idx = 0};

    struct sh_ast_root root = (struct sh_ast_root) {.emptiness = SH_ROOT_EMPTY};

    // No tokens, so an empty root.
    if (ctx.token_count == 0 || tokens[0].type == SH_TOKEN_END) {
        *out = root;
        return SH_PARSE_SUCCESS;
    }

    enum sh_parse_result result = parse_cmd_line(&ctx, &root.cmd_line);
    if (result != SH_PARSE_SUCCESS) {
        return result;
    }

    root.emptiness = SH_ROOT_NONEMPTY;

    // If there are still tokens remaining, that means there are tokens we don't
    // know how to parse.
    if (ctx.token_idx >= ctx.token_count
        || tokens[ctx.token_idx].type != SH_TOKEN_END)
    {
        destroy_ast(&root);
        return SH_PARSE_UNEXPECTED_TOKENS;
    }

    *out = root;
    return SH_PARSE_SUCCESS;
}

void destroy_ast(struct sh_ast_root *ast) {
    if (ast->emptiness == SH_ROOT_EMPTY
        || ast->cmd_line.type == SH_COMMAND_REPEAT)
    {
        return;
    }

    struct sh_ast_cmd_line *cmd_line = &ast->cmd_line;
    assert(cmd_line->type == SH_COMMAND_JOBS);
    destroy_cmd_line(cmd_line);
}

void destroy_simple_cmd(struct sh_ast_simple_cmd *simple_cmd) {
    free(simple_cmd->argv);
}

void destroy_cmd(struct sh_ast_cmd *cmd) {
    free(cmd->redirections);
    struct sh_ast_simple_cmd *simple_cmd = &cmd->simple_cmd;
    destroy_simple_cmd(simple_cmd);
}

void destroy_job(struct sh_ast_job *job) {
    for (size_t idx = 0; idx < job->cmd_count; idx++) {
        struct sh_ast_cmd *cmd = &job->piped_cmds[idx];
        destroy_cmd(cmd);
    }
    free(job->piped_cmds);
}

void destroy_cmd_line(struct sh_ast_cmd_line *cmd_line) {
    for (size_t idx = 0; idx < cmd_line->job_count; idx++) {
        struct sh_job_desc *job_desc = &cmd_line->job_descs[idx];
        struct sh_ast_job *job = &job_desc->job;
        destroy_job(job);
    }
    free(cmd_line->job_descs);
}

enum sh_parse_result
parse_cmd_line(struct sh_parse_context *ctx, struct sh_ast_cmd_line *out) {
    // No tokens left to parse.
    if (ctx->token_idx >= ctx->token_count
        || ctx->tokens[ctx->token_idx].type == SH_TOKEN_END)
    {
        return SH_PARSE_UNEXPECTED_END;
    }

    // Try parsing history (e.g., `!foobar`).
    if (ctx->tokens[ctx->token_idx].type == SH_TOKEN_EXCLAM) {
        ctx->token_idx++;
        if (ctx->token_idx >= ctx->token_count
            || ctx->tokens[ctx->token_idx].type != SH_TOKEN_WORD)
        {
            return SH_PARSE_COMMAND_LINE_FAIL;
        }
        out->type = SH_COMMAND_REPEAT;
        out->repeat_query = ctx->tokens[ctx->token_idx].text;
        ctx->token_idx++;
        return SH_PARSE_SUCCESS;
    }

    // At this point, the command line was not for repeating a command, so try
    // parsing jobs.

    // Allocate memory for the job AST nodes.
    // We start with a capacity of 4 and grow it later if necessary.
    size_t job_descs_capacity = 4;
    struct sh_job_desc *job_descs = malloc(
        sizeof(struct sh_job_desc) * job_descs_capacity
    );
    if (job_descs == NULL) {
        return SH_PARSE_MEMORY_ERROR;
    }
    size_t job_count = 0;

    // A command line has at least one job, so we try parsing the first job.
    struct sh_ast_job job;
    enum sh_parse_result parse_job_result = parse_job(ctx, &job);
    if (parse_job_result != SH_PARSE_SUCCESS) {
        free(job_descs);
        return parse_job_result;
    }

    // At this point, we expect a trailing `&` or `;` and, possibly, more jobs.
    // If there is no `&` or `;`, however, we will not consider it an error
    // here; we will instead leave it to the `parse()` function to check for
    // any remaining unparsed tokens.
    while (parse_job_result == SH_PARSE_SUCCESS
           && ctx->token_idx < ctx->token_count
           && (ctx->tokens[ctx->token_idx].type == SH_TOKEN_AMP
               || ctx->tokens[ctx->token_idx].type == SH_TOKEN_SEMICOLON))
    {
        job_descs[job_count] = (struct sh_job_desc) {
            .type = ctx->tokens[ctx->token_idx].type == SH_TOKEN_AMP
                        ? SH_JOB_BG
                        : SH_JOB_FG,
            .job = job,
        };
        job_count++;
        ctx->token_idx++;

        // Double the size of the jobs array when there is not enough
        // space.
        if (job_count == job_descs_capacity) {
            job_descs_capacity *= 2;
            struct sh_job_desc *tmp = realloc(
                job_descs,
                sizeof(struct sh_job_desc) * job_descs_capacity
            );
            if (tmp == NULL) {
                for (size_t idx = 0; idx < job_count; idx++) {
                    destroy_job(&job_descs[idx].job);
                }
                free(job_descs);
                return SH_PARSE_MEMORY_ERROR;
            }
            job_descs = tmp;
        }

        // Don't try parsing further if there are no more tokens.
        if (ctx->token_idx >= ctx->token_count
            || ctx->tokens[ctx->token_idx].type == SH_TOKEN_END)
        {
            break;
        }

        parse_job_result = parse_job(ctx, &job);
    }

    // If we exited the loop because of an error during parsing of a job, then
    // we need to return the error.
    if (parse_job_result != SH_PARSE_SUCCESS) {
        for (size_t idx = 0; idx < job_count; idx++) {
            destroy_job(&job_descs[idx].job);
        }
        free(job_descs);
        return parse_job_result;
    }

    // Handle the case where the last job does not have a trailing `&` or `;`.
    if (parse_job_result == SH_PARSE_SUCCESS
        && ctx->tokens[ctx->token_idx - 1].type != SH_TOKEN_AMP
        && ctx->tokens[ctx->token_idx - 1].type != SH_TOKEN_SEMICOLON)
    {
        job_descs[job_count] = (struct sh_job_desc) {
            .type = SH_JOB_FG,
            .job = job,
        };
        job_count++;
    }

    out->type = SH_COMMAND_JOBS;
    out->job_descs = job_descs;
    out->job_count = job_count;
    return SH_PARSE_SUCCESS;
}

enum sh_parse_result
parse_job(struct sh_parse_context *ctx, struct sh_ast_job *out) {
    // No tokens to parse.
    if (ctx->token_idx >= ctx->token_count
        || ctx->tokens[ctx->token_idx].type == SH_TOKEN_END)
    {
        return SH_PARSE_UNEXPECTED_END;
    }

    // Allocate memory for the command AST nodes.
    // We start with a capacity of 4 and grow it later if necessary.
    size_t cmds_capacity = 4;
    struct sh_ast_cmd *cmds = malloc(sizeof(struct sh_ast_cmd) * cmds_capacity);
    if (cmds == NULL) {
        return SH_PARSE_MEMORY_ERROR;
    }
    size_t cmd_count = 0;

    // A job has at least one command, so we try parsing the first command.
    struct sh_ast_cmd cmd;
    enum sh_parse_result parse_cmd_result = parse_cmd(ctx, &cmd);
    if (parse_cmd_result != SH_PARSE_SUCCESS) {
        free(cmds);
        return parse_cmd_result;
    }
    cmds[cmd_count] = cmd;
    cmd_count++;

    // If the next symbol is a pipe, then try parsing more commands.
    while (ctx->token_idx < ctx->token_count
           && ctx->tokens[ctx->token_idx].type == SH_TOKEN_PIPE)
    {
        ctx->token_idx++;

        parse_cmd_result = parse_cmd(ctx, &cmd);

        // If the parsing of a command failed, then we don't know how to
        // continue.
        if (parse_cmd_result != SH_PARSE_SUCCESS) {
            free(cmds);
            return parse_cmd_result;
        }

        // Parsing of a command was successful, so add the command's node
        // to the array.
        cmds[cmd_count] = cmd;
        cmd_count++;

        // Grow the command node array if needed.
        if (cmd_count == cmds_capacity) {
            cmds_capacity *= 2;
            struct sh_ast_cmd *tmp = realloc(
                cmds,
                sizeof(struct sh_ast_cmd) * cmds_capacity
            );
            if (tmp == NULL) {
                free(cmds);
                return SH_PARSE_MEMORY_ERROR;
            }
            cmds = tmp;
        }
    }

    out->piped_cmds = cmds;
    out->cmd_count = cmd_count;
    return SH_PARSE_SUCCESS;
}

enum sh_parse_result
parse_cmd(struct sh_parse_context *ctx, struct sh_ast_cmd *out) {
    // No tokens to parse.
    if (ctx->token_idx >= ctx->token_count
        || ctx->tokens[ctx->token_idx].type == SH_TOKEN_END)
    {
        return SH_PARSE_UNEXPECTED_END;
    }

    struct sh_ast_simple_cmd simple_cmd;
    enum sh_parse_result parse_simple_cmd_result = parse_simple_cmd(
        ctx,
        &simple_cmd
    );

    if (parse_simple_cmd_result != SH_PARSE_SUCCESS) {
        return parse_simple_cmd_result;
    }

    struct sh_ast_cmd cmd = (struct sh_ast_cmd) {
        .simple_cmd = simple_cmd,
        .redirection_capacity = 0,
        .redirection_count = 0,
        .redirections = NULL,
    };

    // Parse redirections.
    while (ctx->token_idx < ctx->token_count
           && (ctx->tokens[ctx->token_idx].type == SH_TOKEN_ANGLE_BRACKET_L
               || ctx->tokens[ctx->token_idx].type == SH_TOKEN_ANGLE_BRACKET_R
               || ctx->tokens[ctx->token_idx].type == SH_TOKEN_2_ANGLE_BRACKET_R
           ))

    {
        enum sh_redirect_type redirect_type;
        switch (ctx->tokens[ctx->token_idx].type) {
        case SH_TOKEN_ANGLE_BRACKET_L:
            redirect_type = SH_REDIRECT_STDIN;
            break;
        case SH_TOKEN_ANGLE_BRACKET_R:
            redirect_type = SH_REDIRECT_STDOUT;
            break;
        case SH_TOKEN_2_ANGLE_BRACKET_R:
            redirect_type = SH_REDIRECT_STDERR;
            break;
        default:
            assert(false);
        }

        ctx->token_idx++;
        if (ctx->token_idx >= ctx->token_count
            || ctx->tokens[ctx->token_idx].type == SH_TOKEN_END)
        {
            destroy_cmd(&cmd);
            return SH_PARSE_COMMAND_FAIL;
        }

        char const *redirect_file = ctx->tokens[ctx->token_idx].text;
        ctx->token_idx++;

        // Double the size of the redirections array when there is not enough
        // space.
        if (cmd.redirection_count == cmd.redirection_capacity) {
            size_t new_capacity = cmd.redirection_capacity == 0
                                      ? 2
                                      : cmd.redirection_capacity * 2;

            struct sh_redirection_desc *tmp = realloc(
                cmd.redirections,
                sizeof(struct sh_redirection_desc) * new_capacity
            );

            if (tmp == NULL) {
                destroy_cmd(&cmd);
                return SH_PARSE_MEMORY_ERROR;
            }

            cmd.redirections = tmp;
            cmd.redirection_capacity = new_capacity;
        }

        cmd.redirections[cmd.redirection_count] = (struct sh_redirection_desc) {
            .type = redirect_type,
            .file = redirect_file,
        };
        cmd.redirection_count++;
    }

    *out = cmd;
    return SH_PARSE_SUCCESS;
}

enum sh_parse_result
parse_simple_cmd(struct sh_parse_context *ctx, struct sh_ast_simple_cmd *out) {
    // No tokens to parse.
    if (ctx->token_idx >= ctx->token_count
        || ctx->tokens[ctx->token_idx].type == SH_TOKEN_END)
    {
        return SH_PARSE_UNEXPECTED_END;
    }

    // If the current token is not a word, then we
    // can't parse a simple command.
    if (ctx->tokens[ctx->token_idx].type != SH_TOKEN_WORD) {
        return SH_PARSE_SIMPLE_COMMAND_FAIL;
    }

    // Count the number of `SH_TOKEN_WORDS` first so that we know how much
    // memory to allocate.
    size_t end_idx = ctx->token_idx;
    while (end_idx < ctx->token_count
           && ctx->tokens[end_idx].type == SH_TOKEN_WORD)
    {
        end_idx++;
    }

    // Now, allocate memory for the arguments.
    size_t argc = end_idx - ctx->token_idx;
    // + 1 for the terminating null pointer.
    char const **argv = malloc(sizeof(char *) * (argc + 1));
    if (argv == NULL) {
        return SH_PARSE_MEMORY_ERROR;
    }

    // Fill up `argv`.
    for (size_t idx = ctx->token_idx; idx < end_idx; idx++) {
        assert(ctx->tokens[idx].type == SH_TOKEN_WORD);
        argv[idx - ctx->token_idx] = ctx->tokens[idx].text;
    }

    // Terminating null pointer.
    argv[argc] = NULL;

    // Update the context's index.
    ctx->token_idx = end_idx;
    *out = (struct sh_ast_simple_cmd) {
        .argc = argc,
        .argv = argv,
    };

    return SH_PARSE_SUCCESS;
}

/**
 * Displays a command line AST node for debugging purposes.
 *
 * @param cmd_line pointer to the command line AST node to display
 */
void display_cmd_line(FILE *stream, struct sh_ast_cmd_line *cmd_line);

/**
 * Displays the job AST node for debugging purposes.
 *
 * @param job pointer to the job AST node to display
 */
void display_job(FILE *stream, struct sh_ast_job *job);

/**
 * Displays the command AST node for debugging purposes.
 *
 * @param cmd pointer to the command AST node to display
 */
void display_cmd(FILE *stream, struct sh_ast_cmd *cmd);

/**
 * Displays the simple command AST node for debugging purposes.
 *
 * @param simple_cmd pointer to the simple command AST node to display
 */
void display_simple_cmd(FILE *stream, struct sh_ast_simple_cmd *simple_cmd);

void display_ast(FILE *stream, struct sh_ast_root *ast) {
    fprintf(stream, "ROOT\n");
    if (ast->emptiness == SH_ROOT_NONEMPTY) {
        display_cmd_line(stream, &ast->cmd_line);
    }
}

void display_cmd_line(FILE *stream, struct sh_ast_cmd_line *cmd_line) {
    fprintf(stream, "  COMMAND_LINE\n");
    if (cmd_line->type == SH_COMMAND_REPEAT) {
        fprintf(stream, "    repeat: %s\n", cmd_line->repeat_query);
    } else { // SH_COMMAND_JOBS
        fprintf(stream, "    job count: %lu\n", cmd_line->job_count);
        for (size_t idx = 0; idx < cmd_line->job_count; idx++) {
            fprintf(
                stream,
                "    %s ",
                cmd_line->job_descs[idx].type == SH_JOB_FG ? "FOREGROUND"
                                                           : "BACKGROUND"
            );
            display_job(stream, &cmd_line->job_descs[idx].job);
        }
    }
}

void display_job(FILE *stream, struct sh_ast_job *job) {
    fprintf(stream, "JOB\n");
    fprintf(stream, "      command count: %lu\n", job->cmd_count);
    for (size_t idx = 0; idx < job->cmd_count; idx++) {
        display_cmd(stream, &job->piped_cmds[idx]);
    }
}

void display_cmd(FILE *stream, struct sh_ast_cmd *cmd) {
    fprintf(stream, "      COMMAND:\n");
    display_simple_cmd(stream, &cmd->simple_cmd);
    for (size_t idx = 0; idx < cmd->redirection_count; idx++) {
        fprintf(
            stream,
            "        redirect type: %d\n",
            cmd->redirections[idx].type
        );
        fprintf(
            stream,
            "        redirect file: %s\n",
            cmd->redirections[idx].file
        );
    }
}

void display_simple_cmd(FILE *stream, struct sh_ast_simple_cmd *simple_cmd) {
    fprintf(stream, "        SIMPLE COMMAND\n");
    fprintf(stream, "          argc: %lu\n", simple_cmd->argc);
    fprintf(stream, "          argv: ");
    for (size_t idx = 0; idx < simple_cmd->argc; idx++) {
        fprintf(stream, "%s ", simple_cmd->argv[idx]);
    };
    fprintf(stream, "\n");
}
