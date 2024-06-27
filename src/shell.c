#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "lex.h"
#include "parse.h"
#include "run.h"
#include "shell.h"

int main() {
    setup_signals();

    struct sh_shell_context sh_ctx;
    if (init_shell_context(&sh_ctx) != SH_INIT_SHELL_CONTEXT_SUCCESS) {
        perror("shell context initialisation");
        return EXIT_FAILURE;
    }

    // Main loop.
    bool should_exit = false;
    int exit_code = EXIT_SUCCESS;
    while (!should_exit) {
        printf("%s ", sh_ctx.prompt);

        // Read user input command.
        char *line = NULL;
        size_t line_size = 0; // Will contain the size of the `line`
                              // buffer, not the number of characters!
        ssize_t line_len = getline(&line, &line_size, stdin);
        // `line_len` contains the number of characters (including the
        // delimiter), not the buffer size!

        if (line_len < 0) {
            // According to `getline`'s documentation,
            // `line` must be freed even if `getline` fails.
            free(line);

            perror("getline");

            // Consume the rest of the input in `stdin`.
            char ch;
            do {
                ch = getchar();
            } while (ch != '\n' && ch != EOF);

            continue;
        }

        // Strip trailing newline.
        if (line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
        }

        add_to_history(&sh_ctx, line);

        struct sh_lex_lossless_context lex_lossless_ctx;
        init_lex_lossless_context(line, &lex_lossless_ctx);

        struct sh_lex_refine_context lex_refine_ctx;
        init_lex_refine_context(&lex_refine_ctx);

        struct sh_token token_lossless;
        enum sh_lex_result lex_result_lossless;
        enum sh_lex_result lex_result_refine;
        while (true) {
            lex_result_lossless = lex_lossless(
                &lex_lossless_ctx,
                &token_lossless
            );

            if (lex_result_lossless != SH_LEX_ONGOING) {
                break;
            }

            lex_result_refine = lex_refine(&lex_refine_ctx, &token_lossless);
            destroy_token(&token_lossless);

            if (lex_result_refine != SH_LEX_ONGOING
                && lex_result_refine != SH_LEX_ONGOING)
            {
                break;
            }
        }

        if (lex_result_lossless == SH_LEX_MEMORY_ERROR
            || lex_result_refine == SH_LEX_MEMORY_ERROR)
        {
            printf("error: memory failure\n");
        } else if (lex_result_refine == SH_LEX_UNTERMINATED_QUOTE) {
            printf("error: unterminated quote\n");
        } else {
            struct sh_ast_root ast;
            enum sh_parse_result parse_result = parse(
                lex_refine_ctx.tokbuf,
                lex_refine_ctx.tokbuf_len,
                &ast
            );

            if (parse_result != SH_PARSE_SUCCESS) {
                printf("error: failed to parse command line\n");
            } else {
                struct sh_run_result result = run(&sh_ctx, &ast);
                if (sh_ctx.should_exit) {
                    should_exit = true;
                    exit_code = sh_ctx.exit_code;
                }

                destroy_ast(&ast);
            }
        }

        destroy_lex_refine_context(&lex_refine_ctx);

        // We don't free the line since it is kept in the history.
    }

    destroy_shell_context(&sh_ctx);
    return exit_code;
}

enum sh_init_shell_context_result
init_shell_context(struct sh_shell_context *ctx) {
    // Default prompt is "%".
    char *prompt = strdup("%");
    if (prompt == NULL) {
        return SH_INIT_SHELL_CONTEXT_MEMORY_ERROR;
    }

    *ctx = (struct sh_shell_context) {
        .history_capacity = 0,
        .history_count = 0,
        .history = NULL,
        .prompt = prompt,
    };

    return SH_INIT_SHELL_CONTEXT_SUCCESS;
}

enum sh_add_to_history_result
add_to_history(struct sh_shell_context *ctx, char *line) {
    // Grow the history buffer if needed.
    if (ctx->history_count == ctx->history_capacity) {
        size_t new_capacity = ctx->history_capacity == 0
                                  ? 4
                                  : ctx->history_capacity * 2;

        char **tmp = realloc(ctx->history, sizeof(char *) * new_capacity);
        if (tmp == NULL) {
            return SH_ADD_TO_HISTORY_MEMORY_ERROR;
        }
        ctx->history_capacity = new_capacity;
        ctx->history = tmp;
    }

    // Add the line.
    ctx->history[ctx->history_count] = line;
    ctx->history_count++;
    return SH_ADD_TO_HISTORY_SUCCESS;
}

void destroy_shell_context(struct sh_shell_context *ctx) {
    // Release memory for the history.
    for (size_t idx = 0; idx < ctx->history_count; idx++) {
        free(ctx->history[idx]);
    }
    free(ctx->history);
    ctx->history = NULL;

    // Release memory for the prompt.
    free(ctx->prompt);
}

void setup_signals() {
    struct sigaction sigact_chld;
    sigemptyset(&sigact_chld.sa_mask);
    // Prevent `getline()` from getting interrupted.
    // See `man sigaction` and search for `SA_RESTART`.
    sigact_chld.sa_flags = SA_RESTART;
    sigact_chld.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sigact_chld, NULL);
}

void handle_sigchld(int signo) { waitpid(-1, NULL, WNOHANG); }
