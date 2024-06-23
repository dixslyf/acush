#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "parse.h"

int main() {
    bool cont = true;
    while (cont) {
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

        if (strcmp(line, "exit") == 0) {
            cont = false;
        } else {
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
                struct sh_ast *ast;
                enum sh_parse_result parse_result = parse(
                    tokens,
                    token_count,
                    &ast
                );
                if (parse_result != SH_PARSE_SUCCESS) {
                    printf("Failed to parse command line\n");
                } else {
                    display_ast(ast);
                    destroy_ast_node(ast);
                }
            }
            for (size_t idx = 0; idx < token_count; idx++) {
                destroy_token(&tokens[idx]);
            }
            destroy_lex_context(ctx);
        }

        // `getline` uses dynamic allocation, so we need to free the line.
        free(line);
    }

    return EXIT_SUCCESS;
}
