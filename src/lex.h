#ifndef LEX_H
#define LEX_H

#include <stdbool.h>
#include <stdlib.h>

/** Represents the type of a token. */
enum sh_token_type {
    SH_TOKEN_AMP,               // &
    SH_TOKEN_SEMICOLON,         // ;
    SH_TOKEN_EXCLAM,            // !
    SH_TOKEN_PIPE,              // |
    SH_TOKEN_ANGLE_BRACKET_L,   // <
    SH_TOKEN_ANGLE_BRACKET_R,   // >
    SH_TOKEN_2_ANGLE_BRACKET_R, // 2>
    SH_TOKEN_SINGLE_QUOTE,      // '
    SH_TOKEN_DOUBLE_QUOTE,      // "
    SH_TOKEN_ASTERISK,          // *
    SH_TOKEN_QUESTION,          // ?
    SH_TOKEN_SQUARE_BRACKET_L,  // [
    SH_TOKEN_BACKSLASH,         // `\`
    SH_TOKEN_WHITESPACE,        // A single whitespace character.
    SH_TOKEN_WORD,              // Everything else.
    SH_TOKEN_END,               // Indicates the end of a lex.
};

/**
 * Represents a token, represented as a pair consisting of the token's type and
 * text content.
 */
struct sh_token {
    enum sh_token_type type;
    char *text;
};

/** Keeps track of various context information required by lexing (lossless). */
struct sh_lex_lossless_context {
    // The current character being processed in the input string.
    char const *cp;

    bool finished;
};

/** Keeps track of various context information required by lexing (lossless). */
enum sh_lex_refine_state {
    SH_LEX_REFINE_DULL,
    SH_LEX_REFINE_WORD_QUOTED,
    SH_LEX_REFINE_WORD_QUOTED_END,
    SH_LEX_REFINE_WORD_UNQUOTED,
};

struct sh_lex_refine_context {
    enum sh_lex_refine_state state;

    bool escape;

    struct sh_token start_quote;

    // Buffer for concatenating strings and word sections.
    size_t catbuf_capacity;
    size_t catbuf_len;
    char *catbuf;

    // Queue for multiple tokens.
    size_t tokbuf_capacity;
    size_t tokbuf_len;
    struct sh_token *tokbuf;
};

/** Represents the result of a call to `lex_lossless()`. */
enum sh_lex_result {
    /** Indicates the end of a successful lex. */
    SH_LEX_END,

    /** Indicates that lexing has not yet finished and additional calls to
       `lex()` are required. */
    SH_LEX_ONGOING,

    /** Indicates an error condition where there is a missing closing quote. */
    SH_LEX_UNTERMINATED_QUOTE,

    /** Indicates a failure to allocate memory. */
    SH_LEX_MEMORY_ERROR,

    /** Indicates a failure while expanding globs. */
    SH_LEX_GLOB_ERROR,
};

/**
 * Initialises a lex (lossless) context for the given input string.
 *
 * @param input the input string
 * @param ctx_out a pointer to the context to initialise
 */
void init_lex_lossless_context(
    char const *input,
    struct sh_lex_lossless_context *ctx_out
);

void init_lex_refine_context(struct sh_lex_refine_context *ctx_out);

/**
 * Lexes the given input string into a sequence of tokens.
 *
 * The lex is performed losslessly. That is, it is possible to rebuild the
 * original input exactly from the resulting tokens.
 *
 * This function should be called with a lex context created by
 * `init_lex_lossless_context()`. Each lex should have this function should be
 * called multiple times with the same context. A token is written to
 * `token_out` on every call.
 *
 * For the return value, see `enum sh_lex_result`.
 *
 * @param ctx the lex (lossless) context
 * @param token_out a pointer to write the token to
 *
 * @return the result of the current iteration
 */
enum sh_lex_result
lex_lossless(struct sh_lex_lossless_context *ctx, struct sh_token *token_out);

enum sh_lex_result
lex_refine(struct sh_lex_refine_context *ctx, struct sh_token const *token_in);

void destroy_lex_refine_context(struct sh_lex_refine_context *ctx);

/**
 * Destroys a token.
 *
 * This function should be called on all tokens returned by
 * `lex()` once they are no longer needed.
 *
 * @param token the token to destroy
 */
void destroy_token(struct sh_token *token);

#endif
