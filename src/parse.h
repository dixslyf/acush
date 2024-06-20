#include <stdlib.h>

#define TOKEN_SEPARATORS " \n\t\f\r\v"

/**
 * Parses the given input string into a sequence of tokens
 * delimited by the following characters: space, newline, tab,
 * form feed, carriage return and vertical tab.
 *
 * The number of parsed tokens is returned and the tokens are written to
 * `tokens_out`. `max_tokens` specifies the maximum number of tokens to write
 * to `tokens_out`, and, generally, should be the capacity of `tokens_out`.
 *
 * Note that `input` will be modified!
 *
 * @return the number of tokens parsed
 * @param input the string to parse
 * @param tokens_out an array of strings to write the output tokens to
 * @param max_tokens the maximum number of tokens to write to `tokens_out`
 */
size_t tokenise(char *input, char *tokens_out[], size_t max_tokens);
