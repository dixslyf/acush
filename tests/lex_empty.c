#include <stdlib.h>

#include "assertions.h"
#include "lex.h"

int main() {
  size_t const tokens_size = 4;
  char *tokens[tokens_size];
  ASSERT_EQ(lex("", tokens, tokens_size), 0);
}
