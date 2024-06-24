#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "lex.h"
#include "parse.h"
#include "run.h"

/** Sets up signal handling. */
void setup_signals();

/**
 * Handler for `SIGCHLD` signal.
 *
 * This handler consumes background processes with `waitpid()` to make sure they
 * do not become zombie processes.
 */
void handle_sigchld(int signo);

int main() {
    setup_signals();

    bool should_exit = false;
    int exit_code = EXIT_SUCCESS;
    while (!should_exit) {
        printf("$ ");

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

        struct sh_lex_context *ctx = init_lex_context(line);

        // The worst case scenario for the number of tokens is the
        // number of characters in the line. We waste a bit of
        // space, but the amount is trivial.
        struct sh_token tokens[line_len];
        size_t token_count = 0;

        struct sh_token token;
        enum sh_lex_result lex_result;
        while ((lex_result = lex(ctx, &token)) == SH_LEX_ONGOING) {
            tokens[token_count] = token;
            token_count++;
        }

        if (lex_result == SH_LEX_UNTERMINATED_QUOTE) {
            printf("Error: unterminated quote\n");
        } else {
            struct sh_ast_root ast;
            enum sh_parse_result parse_result = parse(
                tokens,
                token_count,
                &ast
            );

            if (parse_result != SH_PARSE_SUCCESS) {
                printf("Failed to parse command line\n");
            } else {
                display_ast(stderr, &ast);
                sh_run_result result = run(&ast);
                if (result.should_exit) {
                    should_exit = true;
                    exit_code = result.exit_code;
                }

                destroy_ast(&ast);
            }
        }

        for (size_t idx = 0; idx < token_count; idx++) {
            destroy_token(&tokens[idx]);
        }
        destroy_lex_context(ctx);

        // `getline` uses dynamic allocation, so we need to free the line.
        free(line);
    }

    return exit_code;
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
