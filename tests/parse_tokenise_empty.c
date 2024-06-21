#include <stdlib.h>

#include "assertions.h"
#include "parse.h"

int main() {
  size_t const tokens_size = 4;
  char *tokens[tokens_size];
  ASSERT_EQ(tokenise("", tokens, tokens_size), 0);
}
