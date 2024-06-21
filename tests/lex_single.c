#include <stdlib.h>
#include <string.h>

#include "assertions.h"
#include "lex.h"

int main() {
  char input[7] = "foo123";
  size_t tokens_size = 4;
  char *tokens[tokens_size];

  ASSERT_EQ(lex(input, tokens, tokens_size), 1);
  ASSERT_EQ(strcmp(tokens[0], input), 0);
}
