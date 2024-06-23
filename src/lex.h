#ifndef LEX_H
#define LEX_H

#include <stdlib.h>

enum sh_token_type {
  SH_TOKEN_AMP,               // &
  SH_TOKEN_SEMICOLON,         // ;
  SH_TOKEN_EXCLAM,            // !
  SH_TOKEN_PIPE,              // |
  SH_TOKEN_ANGLE_BRACKET_L,   // <
  SH_TOKEN_ANGLE_BRACKET_R,   // >
  SH_TOKEN_2_ANGLE_BRACKET_R, // 2>
  SH_TOKEN_WORD,              // Everything else.
};

struct sh_token {
  enum sh_token_type type;
  char *text;
};

/**
 * Keeps track of various context information required by lexing.
 *
 * See `lex()` for more information.
 */
struct sh_lex_context;

/**
 * Represents the result of a call to `lex()`.
 */
enum sh_lex_result {
  /** Indicates a successful lex. */
  SH_LEX_END,

  /** Indicates that lexing has not yet finished and additional calls to `lex()`
     are required. */
  SH_LEX_ONGOING,

  /** Indicates an error condition where there is a missing closing quote. */
  SH_LEX_UNTERMINATED_QUOTE,
};

/** Initialises a lex context for the given input string. */
struct sh_lex_context *init_lex_context(char const *input);

/**
 * Destroys a lex context.
 *
 * This function should be called on any lex contexts created by
 * `init_lex_context()` once the context is no longer needed.
 */
void destroy_lex_context(struct sh_lex_context *ctx);

/**
 * Lexes the given input string into a sequence of tokens
 * delimited by the following characters: space, newline, tab,
 * form feed, carriage return and vertical tab.
 *
 * Each lex should have this function should be called multiple times with the
 * same context. A token is written to `token_out` on every call, except when
 * the lex has finished.
 *
 * For the return value, see `enum sh_lex_result`.
 */
enum sh_lex_result lex(struct sh_lex_context *ctx, struct sh_token *token_out);

/**
 * Destroys a token.
 *
 * This function should be called on all tokens returned by
 * `lex()` once they are no longer needed.
 */
void destroy_token(struct sh_token *token);

#endif
