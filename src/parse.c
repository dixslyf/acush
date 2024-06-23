#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "lex.h"
#include "parse.h"

struct sh_parse_context {
  struct sh_token *tokens;
  size_t token_count;
  size_t token_idx;
};

struct sh_ast *init_ast_node(enum sh_ast_type type, union sh_ast_data data);
enum sh_parse_result parse_command_line(struct sh_parse_context *ctx,
                                        struct sh_ast **ast_out);
enum sh_parse_result parse_job(struct sh_parse_context *ctx,
                               struct sh_ast **ast_out);
enum sh_parse_result parse_command(struct sh_parse_context *ctx,
                                   struct sh_ast **ast_out);
enum sh_parse_result parse_simple_command(struct sh_parse_context *ctx,
                                          struct sh_ast **ast_out);

enum sh_parse_result parse(struct sh_token tokens[], size_t token_count,
                           struct sh_ast **ast_out) {
  struct sh_parse_context ctx = {
      .tokens = tokens, .token_count = token_count, .token_idx = 0};

  struct sh_ast *root_node = init_ast_node(
      SH_AST_ROOT, (union sh_ast_data){.root.command_line = NULL});
  if (root_node == NULL) {
    return SH_PARSE_MEMORY_ERROR;
  }

  // No tokens, so an empty root.
  if (ctx.token_count == 0) {
    *ast_out = root_node;
    return SH_PARSE_SUCCESS;
  }

  struct sh_ast *command_line_node;
  enum sh_parse_result result = parse_command_line(&ctx, &command_line_node);
  if (result != SH_PARSE_SUCCESS) {
    return result;
  }

  if (ctx.token_idx < ctx.token_count) {
    return SH_PARSE_UNEXPECTED_TOKENS;
  }

  root_node->data.root.command_line = command_line_node;
  *ast_out = root_node;
  return SH_PARSE_SUCCESS;
}

struct sh_ast *init_ast_node(enum sh_ast_type type, union sh_ast_data data) {
  struct sh_ast *node = malloc(sizeof(struct sh_ast));
  if (node == NULL) {
    return NULL;
  }
  node->type = type;
  node->data = data;
  return node;
}

void destroy_ast_node(struct sh_ast *ast) {
  switch (ast->type) {
  case SH_AST_ROOT:
    if (ast->data.root.command_line != NULL) {
      destroy_ast_node(ast->data.root.command_line);
    }
    break;
  case SH_AST_COMMAND_LINE:
    for (size_t idx = 0; idx < ast->data.command_line.job_count; idx++) {
      destroy_ast_node(ast->data.command_line.jobs[idx].ast_node);
    }
    break;
  case SH_AST_JOB:
    for (size_t idx = 0; idx < ast->data.job.cmd_count; idx++) {
      destroy_ast_node(ast->data.job.piped_cmds[idx]);
    }
    break;
  case SH_AST_COMMAND:
    destroy_ast_node(ast->data.command.simple_command);
    break;
  case SH_AST_SIMPLE_COMMAND:
    free(ast->data.simple_command.argv);
    break;
  }
  free(ast);
}

enum sh_parse_result parse_command_line(struct sh_parse_context *ctx,
                                        struct sh_ast **ast_out) {
  // No tokens to parse.
  if (ctx->token_idx >= ctx->token_count) {
    return SH_PARSE_UNEXPECTED_END;
  }

  // Try parsing history (e.g., `!foobar`).
  if (ctx->tokens[ctx->token_idx].type == SH_TOKEN_EXCLAM &&
      ctx->tokens[ctx->token_idx + 1].type == SH_TOKEN_WORD) {
    struct sh_ast *node = init_ast_node(
        SH_AST_COMMAND_LINE,
        (union sh_ast_data){
            .command_line = {.type = SH_COMMAND_REPEAT,
                             .repeat = ctx->tokens[ctx->token_idx + 1].text}});

    if (node == NULL) {
      return SH_PARSE_MEMORY_ERROR;
    }

    *ast_out = node;
    return SH_PARSE_SUCCESS;
  }

  // At this point, the command line was not for repeating a command, so try
  // parsing jobs.

  // Allocate memory for the job AST nodes.
  // We start with a capacity of 4 and grow it later if necessary.
  size_t jobs_capacity = 4;
  struct sh_job *jobs = malloc(sizeof(struct sh_job) * jobs_capacity);
  if (jobs == NULL) {
    return SH_PARSE_MEMORY_ERROR;
  }
  size_t job_count = 0;

  // A command line has at least one job, so we try parsing the first job.
  struct sh_ast *job_node;
  enum sh_parse_result parse_job_result = parse_job(ctx, &job_node);
  if (parse_job_result != SH_PARSE_SUCCESS) {
    return parse_job_result;
  }

  // At this point, we expect a trailing `&` or `;` and, possibly, more jobs.
  // If there is no `&` or `;`, however, we will not consider it an error here;
  // we will instead leave it to the `parse()` function to check for any
  // remaining unparsed tokens.
  while (parse_job_result == SH_PARSE_SUCCESS &&
         ctx->token_idx < ctx->token_count &&
         (ctx->tokens[ctx->token_idx].type == SH_TOKEN_AMP ||
          ctx->tokens[ctx->token_idx].type == SH_TOKEN_SEMICOLON)) {
    enum sh_job_type job_type = ctx->tokens[ctx->token_idx].type == SH_TOKEN_AMP
                                    ? SH_JOB_BG
                                    : SH_JOB_FG;
    ctx->token_idx++;

    jobs[job_count] =
        (struct sh_job){.ast_node = job_node, .job_type = job_type};
    job_count++;

    // Double the size of the jobs array when there is not enough space.
    if (job_count == jobs_capacity) {
      jobs_capacity *= 2;
      struct sh_job *jobs_tmp =
          realloc(jobs, sizeof(struct sh_job) * jobs_capacity);
      if (jobs_tmp == NULL) {
        free(jobs);
        return SH_PARSE_MEMORY_ERROR;
      }
      jobs = jobs_tmp;
    }

    // Don't try parsing further if there are no more tokens.
    if (ctx->token_idx >= ctx->token_count) {
      break;
    }
    parse_job_result = parse_job(ctx, &job_node);
  }

  // If we exited the loop because of an error during parsing of a job, then we
  // need to return the error.
  if (parse_job_result != SH_PARSE_SUCCESS) {
    return parse_job_result;
  }

  // Handle the case where the last job does not have a trailing `&` or `;`.
  if (parse_job_result == SH_PARSE_SUCCESS &&
      jobs[job_count - 1].ast_node != job_node) {
    jobs[job_count] =
        (struct sh_job){.ast_node = job_node, .job_type = SH_JOB_FG};
    job_count++;
  }

  struct sh_ast *node = init_ast_node(
      SH_AST_COMMAND_LINE,
      (union sh_ast_data){.command_line = {.type = SH_COMMAND_JOBS,
                                           .jobs = jobs,
                                           .job_count = job_count}});
  if (node == NULL) {
    return SH_PARSE_MEMORY_ERROR;
  }

  *ast_out = node;
  return SH_PARSE_SUCCESS;
}

enum sh_parse_result parse_job(struct sh_parse_context *ctx,
                               struct sh_ast **ast_out) {
  // No tokens to parse.
  if (ctx->token_idx >= ctx->token_count) {
    return SH_PARSE_UNEXPECTED_END;
  }

  // Allocate memory for the command AST nodes.
  // We start with a capacity of 4 and grow it later if necessary.
  size_t cmds_capacity = 4;
  struct sh_ast **cmds = malloc(sizeof(struct sh_ast *) * cmds_capacity);
  if (cmds == NULL) {
    return SH_PARSE_MEMORY_ERROR;
  }
  size_t cmd_count = 0;

  // A job has at least one command, so we try parsing the first command.
  struct sh_ast *cmd_node;
  enum sh_parse_result parse_cmd_result = parse_command(ctx, &cmd_node);
  if (parse_cmd_result != SH_PARSE_SUCCESS) {
    return parse_cmd_result;
  }
  cmds[cmd_count] = cmd_node;
  cmd_count++;

  // If the next symbol is a pipe, then try parsing more commands.
  while (ctx->token_idx < ctx->token_count &&
         ctx->tokens[ctx->token_idx].type == SH_TOKEN_PIPE) {
    ctx->token_idx++;

    parse_cmd_result = parse_command(ctx, &cmd_node);

    // If the parsing of a command failed, then we don't know how to continue.
    if (parse_cmd_result != SH_PARSE_SUCCESS) {
      return parse_cmd_result;
    }

    // Parsing of a command was successful, so add the command's node to the
    // array.
    cmds[cmd_count] = cmd_node;
    cmd_count++;

    // Grow the command node array if needed.
    if (cmd_count == cmds_capacity) {
      cmds_capacity *= 2;
      struct sh_ast **cmds_tmp =
          realloc(cmds, sizeof(struct sh_ast *) * cmds_capacity);
      if (cmds_tmp == NULL) {
        free(cmds);
        return SH_PARSE_MEMORY_ERROR;
      }
      cmds = cmds_tmp;
    }
  }

  struct sh_ast *node = init_ast_node(
      SH_AST_JOB,
      (union sh_ast_data){.job = {.piped_cmds = cmds, .cmd_count = cmd_count}});
  if (node == NULL) {
    free(cmds);
    return SH_PARSE_MEMORY_ERROR;
  }

  *ast_out = node;
  return SH_PARSE_SUCCESS;
}

enum sh_parse_result parse_command(struct sh_parse_context *ctx,
                                   struct sh_ast **ast_out) {
  // No tokens to parse.
  if (ctx->token_idx >= ctx->token_count) {
    return SH_PARSE_UNEXPECTED_END;
  }

  struct sh_ast *simple_command_node;
  enum sh_parse_result parse_simple_cmd_result =
      parse_simple_command(ctx, &simple_command_node);
  if (parse_simple_cmd_result != SH_PARSE_SUCCESS) {
    return parse_simple_cmd_result;
  }

  struct sh_ast *node = init_ast_node(
      SH_AST_COMMAND,
      (union sh_ast_data){.command = {.simple_command = simple_command_node,
                                      .redirect_type = SH_REDIRECT_NONE,
                                      .redirect_file = NULL}});
  if (node == NULL) {
    return SH_PARSE_MEMORY_ERROR;
  }

  struct sh_token *token = &ctx->tokens[ctx->token_idx];
  if (!(token->type == SH_TOKEN_ANGLE_BRACKET_L ||
        token->type == SH_TOKEN_ANGLE_BRACKET_R ||
        token->type == SH_TOKEN_2_ANGLE_BRACKET_R)) {
    *ast_out = node;
    return SH_PARSE_SUCCESS;
  }

  if (ctx->tokens[ctx->token_idx + 1].type == SH_TOKEN_WORD) {
    switch (token->type) {
    case SH_TOKEN_ANGLE_BRACKET_L:
      node->data.command.redirect_type = SH_REDIRECT_STDIN;
      break;
    case SH_TOKEN_ANGLE_BRACKET_R:
      node->data.command.redirect_type = SH_REDIRECT_STDOUT;
      break;
    case SH_TOKEN_2_ANGLE_BRACKET_R:
      node->data.command.redirect_type = SH_REDIRECT_STDERR;
      break;
    default:
      assert(false);
    }

    node->data.command.redirect_file = ctx->tokens[ctx->token_idx + 1].text;
    *ast_out = node;
    ctx->token_idx += 2;
    return SH_PARSE_SUCCESS;
  }

  return SH_PARSE_COMMAND_FAIL;
}

enum sh_parse_result parse_simple_command(struct sh_parse_context *ctx,
                                          struct sh_ast **ast_out) {
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
  while (ctx->tokens[end_idx].type == SH_TOKEN_WORD) {
    end_idx++;
  }

  // Now, allocate memory for the arguments.
  size_t argc = end_idx - ctx->token_idx;
  char **argv = malloc(sizeof(char *) * argc);
  if (argv == NULL) {
    return SH_PARSE_MEMORY_ERROR;
  }

  // Fill up `argv`.
  for (size_t idx = ctx->token_idx; idx < end_idx; idx++) {
    assert(ctx->tokens[idx].type == SH_TOKEN_WORD);
    argv[idx - ctx->token_idx] = ctx->tokens[idx].text;
  }

  // Allocate and create the AST node.
  struct sh_ast *node = init_ast_node(
      SH_AST_SIMPLE_COMMAND,
      (union sh_ast_data){.simple_command = {.argc = argc, .argv = argv}});
  if (node == NULL) {
    return SH_PARSE_MEMORY_ERROR;
  }

  // Update the context's index.
  ctx->token_idx = end_idx;
  *ast_out = node;

  return SH_PARSE_SUCCESS;
}

void display_ast(struct sh_ast *ast) {
  switch (ast->type) {
  case SH_AST_ROOT:
    printf("ROOT\n");
    if (ast->data.root.command_line != NULL) {
      display_ast(ast->data.root.command_line);
    }
    break;
  case SH_AST_COMMAND_LINE:
    printf("  COMMAND_LINE\n");
    if (ast->data.command_line.type == SH_COMMAND_REPEAT) {
      printf("    repeat: %s\n", ast->data.command_line.repeat);
    } else { // SH_COMMAND_JOBS
      printf("    job count: %lu\n", ast->data.command_line.job_count);
      for (size_t idx; idx < ast->data.command_line.job_count; idx++) {
        printf("    %s ", ast->data.command_line.jobs[idx].job_type == SH_JOB_FG
                              ? "FOREGROUND"
                              : "BACKGROUND");
        display_ast(ast->data.command_line.jobs[idx].ast_node);
      }
    }
    break;
  case SH_AST_JOB:
    printf("JOB\n");
    printf("      command count: %lu\n", ast->data.job.cmd_count);
    for (size_t idx = 0; idx < ast->data.job.cmd_count; idx++) {
      display_ast(ast->data.job.piped_cmds[idx]);
    }
    break;
  case SH_AST_COMMAND:
    printf("      COMMAND:\n");
    display_ast(ast->data.command.simple_command);
    printf("        redirect type: %d\n", ast->data.command.redirect_type);
    printf("        redirect file: %s\n", ast->data.command.redirect_file);
    break;
  case SH_AST_SIMPLE_COMMAND:
    printf("        SIMPLE COMMAND\n");
    printf("          argc: %lu\n", ast->data.simple_command.argc);
    printf("          argv: ");
    for (size_t idx = 0; idx < ast->data.simple_command.argc; idx++) {
      printf("%s ", ast->data.simple_command.argv[idx]);
    };
    printf("\n");
    break;
  }
}
