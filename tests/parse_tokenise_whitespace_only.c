#include <stdlib.h>

#include "assertions.h"
#include "parse.h"

int main() {
  char input[7] = TOKEN_SEPARATORS;
  size_t const tokens_size = 1;
  char *tokens[tokens_size];
  ASSERT_EQ(tokenise("", tokens, tokens_size), 0);
}
