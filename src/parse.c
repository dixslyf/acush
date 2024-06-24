#include "parse.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "lex.h"

struct sh_parse_context {
    struct sh_token *tokens;
    size_t token_count;
    size_t token_idx;
};

void destroy_simple_cmd(struct sh_ast_simple_cmd *simple_cmd);
void destroy_cmd(struct sh_ast_cmd *cmd);
void destroy_job(struct sh_ast_job *job);
void destroy_cmd_line(struct sh_ast_cmd_line *cmd_line);

enum sh_parse_result
parse_cmd_line(struct sh_parse_context *ctx, struct sh_ast_cmd_line *ast_out);

enum sh_parse_result
parse_job(struct sh_parse_context *ctx, struct sh_ast_job *ast_out);

enum sh_parse_result
parse_cmd(struct sh_parse_context *ctx, struct sh_ast_cmd *ast_out);

enum sh_parse_result parse_simple_cmd(
    struct sh_parse_context *ctx,
    struct sh_ast_simple_cmd *ast_out
);

enum sh_parse_result
parse(struct sh_token tokens[], size_t token_count, struct sh_ast_root *out) {
    struct sh_parse_context ctx =
        {.tokens = tokens, .token_count = token_count, .token_idx = 0};

    struct sh_ast_root root = (struct sh_ast_root) {.emptiness = SH_ROOT_EMPTY};

    // No tokens, so an empty root.
    if (ctx.token_count == 0) {
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
    if (ctx.token_idx < ctx.token_count) {
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
    // No tokens to parse.
    if (ctx->token_idx >= ctx->token_count) {
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
        out->repeat = ctx->tokens[ctx->token_idx + 1].text;
        ctx->token_idx += 2;
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
        if (ctx->token_idx >= ctx->token_count) {
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
    if (ctx->token_idx >= ctx->token_count) {
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
    if (ctx->token_idx >= ctx->token_count) {
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
        .redirect_type = SH_REDIRECT_NONE,
        .redirect_file = NULL,
    };

    // At this point, the `<`, `>` or `2>` are optional.

    // If there are no tokens remaining, or there are and the next is one of
    // `<`,
    // `>` or `2>`, then there is no redirection.
    if (ctx->token_idx >= ctx->token_count
        || !(
            ctx->tokens[ctx->token_idx].type == SH_TOKEN_ANGLE_BRACKET_L
            || ctx->tokens[ctx->token_idx].type == SH_TOKEN_ANGLE_BRACKET_R
            || ctx->tokens[ctx->token_idx].type == SH_TOKEN_2_ANGLE_BRACKET_R
        ))
    {
        *out = cmd;
        return SH_PARSE_SUCCESS;
    }

    // The next token is one of `<`, `>` or `2>`, so we need to check the token
    // after that to get the redirect file path.
    if (ctx->token_idx + 1 < ctx->token_count
        && ctx->tokens[ctx->token_idx + 1].type == SH_TOKEN_WORD)
    {
        switch (ctx->tokens[ctx->token_idx].type) {
        case SH_TOKEN_ANGLE_BRACKET_L:
            cmd.redirect_type = SH_REDIRECT_STDIN;
            break;
        case SH_TOKEN_ANGLE_BRACKET_R:
            cmd.redirect_type = SH_REDIRECT_STDOUT;
            break;
        case SH_TOKEN_2_ANGLE_BRACKET_R:
            cmd.redirect_type = SH_REDIRECT_STDERR;
            break;
        default:
            assert(false);
        }

        cmd.redirect_file = ctx->tokens[ctx->token_idx + 1].text;
        *out = cmd;
        ctx->token_idx += 2;
        return SH_PARSE_SUCCESS;
    }

    // If we've reached this point, that means there was a `<`, `>` or `2>`
    // token, but no redirect file path.
    destroy_cmd(&cmd);
    return SH_PARSE_COMMAND_FAIL;
}

enum sh_parse_result
parse_simple_cmd(struct sh_parse_context *ctx, struct sh_ast_simple_cmd *out) {
    // No tokens to parse.
    if (ctx->token_idx >= ctx->token_count) {
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
    char **argv = malloc(sizeof(char *) * (argc + 1));
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

void display_cmd_line(struct sh_ast_cmd_line *cmd_line);
void display_job(struct sh_ast_job *job);
void display_cmd(struct sh_ast_cmd *cmd);
void display_simple_cmd(struct sh_ast_simple_cmd *simple_cmd);

void display_ast(struct sh_ast_root *ast) {
    printf("ROOT\n");
    if (ast->emptiness == SH_ROOT_NONEMPTY) {
        display_cmd_line(&ast->cmd_line);
    }
}

void display_cmd_line(struct sh_ast_cmd_line *cmd_line) {
    printf("  COMMAND_LINE\n");
    if (cmd_line->type == SH_COMMAND_REPEAT) {
        printf("    repeat: %s\n", cmd_line->repeat);
    } else { // SH_COMMAND_JOBS
        printf("    job count: %lu\n", cmd_line->job_count);
        for (size_t idx = 0; idx < cmd_line->job_count; idx++) {
            printf(
                "    %s ",
                cmd_line->job_descs[idx].type == SH_JOB_FG ? "FOREGROUND"
                                                           : "BACKGROUND"
            );
            display_job(&cmd_line->job_descs[idx].job);
        }
    }
}

void display_job(struct sh_ast_job *job) {
    printf("JOB\n");
    printf("      command count: %lu\n", job->cmd_count);
    for (size_t idx = 0; idx < job->cmd_count; idx++) {
        display_cmd(&job->piped_cmds[idx]);
    }
}
void display_cmd(struct sh_ast_cmd *cmd) {
    printf("      COMMAND:\n");
    display_simple_cmd(&cmd->simple_cmd);
    printf("        redirect type: %d\n", cmd->redirect_type);
    printf("        redirect file: %s\n", cmd->redirect_file);
}
void display_simple_cmd(struct sh_ast_simple_cmd *simple_cmd) {
    printf("        SIMPLE COMMAND\n");
    printf("          argc: %lu\n", simple_cmd->argc);
    printf("          argv: ");
    for (size_t idx = 0; idx < simple_cmd->argc; idx++) {
        printf("%s ", simple_cmd->argv[idx]);
    };
    printf("\n");
}
