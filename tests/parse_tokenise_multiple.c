#include <stdlib.h>
#include <string.h>

#include "assertions.h"
#include "parse.h"

int main() {
  char input[56] = "a bc\tdef\nghij\vklmno\fpqrstu\rvwxyzab";
  size_t tokens_size = 7;
  char *tokens[tokens_size];
  size_t token_count = tokenise(input, tokens, tokens_size);
  ASSERT_EQ(token_count, tokens_size);
  ASSERT_EQ(strcmp(tokens[0], "a"), 0);
  ASSERT_EQ(strcmp(tokens[1], "bc"), 0);
  ASSERT_EQ(strcmp(tokens[2], "def"), 0);
  ASSERT_EQ(strcmp(tokens[3], "ghij"), 0);
  ASSERT_EQ(strcmp(tokens[4], "klmno"), 0);
  ASSERT_EQ(strcmp(tokens[5], "pqrstu"), 0);
  ASSERT_EQ(strcmp(tokens[6], "vwxyzab"), 0);
}
