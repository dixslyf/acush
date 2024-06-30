/**
 * @file lex.h
 *
 * Declarations for raw lexing.
 *
 * Unlike "normal" lexing, raw lexing is lossless — it is possible to construct
 * the original input from the resulting tokens. Raw lexing should not be
 * performed directly by the shell, however, and is intended to be an
 * implementation detail of lexing.
 */

#ifndef RAW_LEX_H
#define RAW_LEX_H

#include <stdbool.h>

/** Represents the type of a raw token. */
enum sh_raw_token_type {
    SH_RAW_TOKEN_AMP,               // &
    SH_RAW_TOKEN_SEMICOLON,         // ;
    SH_RAW_TOKEN_EXCLAM,            // !
    SH_RAW_TOKEN_PIPE,              // |
    SH_RAW_TOKEN_ANGLE_BRACKET_L,   // <
    SH_RAW_TOKEN_ANGLE_BRACKET_R,   // >
    SH_RAW_TOKEN_2_ANGLE_BRACKET_R, // 2>
    SH_RAW_TOKEN_SINGLE_QUOTE,      // '
    SH_RAW_TOKEN_DOUBLE_QUOTE,      // "
    SH_RAW_TOKEN_ASTERISK,          // *
    SH_RAW_TOKEN_QUESTION,          // ?
    SH_RAW_TOKEN_SQUARE_BRACKET_L,  // [
    SH_RAW_TOKEN_BACKSLASH,         // `\`
    SH_RAW_TOKEN_WHITESPACE,        // A single whitespace character.
    SH_RAW_TOKEN_TEXT,              // Everything else.
    SH_RAW_TOKEN_END,               // Indicates the end of a lex.
};

/**
 * Represents a raw token.
 *
 * Each raw token is a pair consisting of its type and text content.
 */
struct sh_raw_token {
    enum sh_raw_token_type type;
    char const *text;
};

/** Keeps track of context information required by a raw lex. */
struct sh_raw_lex_context {
    /** The current character being processed in the input string. */
    char const *cp;

    /** Keeps track of whether lexing has ended. */
    bool finished;
};

/**
 * Initialises a raw lex context for the given input string.
 *
 * @param input the input string
 * @param ctx_out a pointer to the context to initialise
 */
void init_raw_lex_context(
    struct sh_raw_lex_context *ctx_out,
    char const *input
);

/** Represents the result of a call to `raw_lex()`. */
enum sh_raw_lex_result {
    /** Indicates the end of a successful lex. */
    SH_RAW_LEX_END,

    /** Indicates that lexing has not yet finished and additional calls to
       `raw_lex()` are required. */
    SH_RAW_LEX_ONGOING,

    /** Indicates a failure to allocate memory. */
    SH_RAW_LEX_MEMORY_ERROR,

    /** Indicates a failure while expanding globs. */
    SH_RAW_LEX_GLOB_ERROR,
};

/**
 * Lexes an input string specified by the context into a sequence of raw tokens.
 *
 * The lex is performed losslessly. That is, it is possible to rebuild the
 * original input exactly from the resulting tokens.
 *
 * This function is reentrant and should be called with a lex context
 * initialised by `init_raw_lex_context()`. Each lex should have this function
 * called multiple times with the same context. A token is written to
 * `token_out` on every call.
 *
 * For the return value, see `enum sh_raw_lex_result`.
 *
 * @param ctx the raw lex context
 * @param token_out a pointer to write the token to
 *
 * @return the result of the current iteration
 */
enum sh_raw_lex_result
raw_lex(struct sh_raw_lex_context *ctx, struct sh_raw_token *token_out);

/**
 * Destroys a raw token.
 *
 * This function should be called on all tokens returned by
 * `raw_lex()` once they are no longer needed.
 *
 * @param token the raw token to destroy
 */
void destroy_raw_token(struct sh_raw_token *token);

#endif
