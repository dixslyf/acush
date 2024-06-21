#include <stdlib.h>
#include <string.h>

#include "assertions.h"
#include "parse.h"

int main() {
  for (size_t idx = 0; idx < strlen(TOKEN_SEPARATORS); idx++) {
    char input[5] = "foo";
    input[3] = TOKEN_SEPARATORS[idx];
    input[4] = '\0';

    char *tokens[2];
    size_t tokens_size = 2;

    ASSERT_EQ(tokenise(input, tokens, tokens_size), 1);
    ASSERT_EQ(strcmp(tokens[0], "foo"), 0);
    // We don't want `tokenise` to be returning empty strings.
  }
}
