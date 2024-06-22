#include <stdlib.h>

struct sh_token {
  enum {
    SH_TOKEN_AMP,               // &
    SH_TOKEN_SEMICOLON,         // ;
    SH_TOKEN_EXCLAM,            // !
    SH_TOKEN_PIPE,              // |
    SH_TOKEN_ANGLE_BRACKET_L,   // <
    SH_TOKEN_ANGLE_BRACKET_R,   // >
    SH_TOKEN_2_ANGLE_BRACKET_R, // 2>
    SH_TOKEN_WORD,              // Everything else.
  } type;
  char *text;
};

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
ssize_t lex(char const *input, struct sh_token tokens_out[], size_t max_tokens);
