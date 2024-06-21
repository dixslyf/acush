#include <stdlib.h>

#include "assertions.h"
#include "lex.h"

int main() {
  char input[7] = TOKEN_SEPARATORS;
  size_t const tokens_size = 1;
  char *tokens[tokens_size];
  ASSERT_EQ(lex("", tokens, tokens_size), 0);
}
