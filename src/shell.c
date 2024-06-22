#include "lex.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
  bool cont = true;
  while (cont) {
    printf("$ ");

    // Read user input command.
    char *line = NULL;
    size_t line_size = 0; // Will contain the size of the `line` buffer, not the
                          // number of characters!
    ssize_t line_len = getline(&line, &line_size, stdin);
    // `line_len` contains the number of characters (including the delimiter),
    // not the buffer size!

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

      struct sh_token token;

      enum sh_lex_result result;
      while ((result = lex(ctx, &token)) == SH_LEX_ONGOING) {
        // FIXME: This is just for verification. Remove this later!
        printf("Token: %u %s\n", token.type, token.text);
        destroy_token(&token);
      }

      if (result == SH_LEX_UNTERMINATED_QUOTE) {
        printf("Error: unterminated quote\n");
      }

      destroy_lex_context(ctx);
    }

    // `getline` uses dynamic allocation, so we need to free the line.
    free(line);
  }

  return EXIT_SUCCESS;
}
