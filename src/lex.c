#include <string.h>

#include "lex.h"

size_t lex(char *input, char *tokens_out[], size_t max_tokens) {
  // See `man 3 strtok_r`.
  char *saveptr = NULL;
  char *tok = strtok_r(input, TOKEN_SEPARATORS, &saveptr);

  size_t idx = 0;
  while (tok != NULL && idx < max_tokens) {
    tokens_out[idx] = tok;
    tok = strtok_r(NULL, TOKEN_SEPARATORS, &saveptr);
    idx++;
  }

  // `idx` also happens to be the number of tokens.
  return idx;
}
