#include <stdlib.h>
#include <string.h>

#include "assertions.h"
#include "lex.h"

int main() {
  for (size_t idx = 0; idx < strlen(TOKEN_SEPARATORS); idx++) {
    char input[5] = "foo";
    input[3] = TOKEN_SEPARATORS[idx];
    input[4] = '\0';

    char *tokens[2];
    size_t tokens_size = 2;

    ASSERT_EQ(lex(input, tokens, tokens_size), 1);
    ASSERT_EQ(strcmp(tokens[0], "foo"), 0);
    // We don't want `lex` to be returning empty strings.
  }
}
