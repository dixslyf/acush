#include "parse.h"

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
      // The upper bound on the number of tokens is the number of
      // characters in the line. Most of the time, we'll be wasting
      // a bit of memory, but the amount wasted is trivial.
      char *tokens[line_len];
      int num_tokens = tokenise(line, tokens, line_len);

      // FIXME: This is just for verification. Remove this later!
      for (size_t idx = 0; idx < num_tokens; idx++) {
        printf("Token %lu: %s\n", idx, tokens[idx]);
      }
    }

    // `getline` uses dynamic allocation, so we need to free the line.
    free(line);
  }

  return EXIT_SUCCESS;
}
