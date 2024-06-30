/**
 * @file lex.h
 *
 * Declarations for lexing.
 */

#ifndef LEX_H
#define LEX_H

#include <stdbool.h>
#include <stdlib.h>

#include "raw_lex.h"

/** Represents the type of a token. */
enum sh_token_type {
    SH_TOKEN_AMP,               // &
    SH_TOKEN_SEMICOLON,         // ;
    SH_TOKEN_EXCLAM,            // !
    SH_TOKEN_PIPE,              // |
    SH_TOKEN_ANGLE_BRACKET_L,   // <
    SH_TOKEN_ANGLE_BRACKET_R,   // >
    SH_TOKEN_2_ANGLE_BRACKET_R, // 2>
    SH_TOKEN_WORD, // Combination of consecutive quoted strings and text.
    SH_TOKEN_END,  // Indicates the end of a lex.
};

/**
 * Represents a token.
 *
 * Each token is a pair consisting of its type and text content.
 */
struct sh_token {
    enum sh_token_type type;
    char const *text;
};

/** Represents the possible states of the lexer. */
enum sh_lex_state {
    SH_LEX_STATE_DULL,        /**< Not in a quoted string, unquoted section of a
                                  word or at the closing quote for a string. */
    SH_LEX_STATE_WORD_QUOTED, /**< In a quoted string. */
    SH_LEX_STATE_WORD_QUOTED_END, /**< At the closing quote for a string. */
    SH_LEX_STATE_WORD_UNQUOTED,   /**< In an unquoted section of a word. */
};

/** Keeps track of various context information required by a lex. */
struct sh_lex_context {
    /** Buffer for storing the output tokens. */
    size_t tokbuf_capacity;
    size_t tokbuf_len;
    struct sh_token *tokbuf;

    /** Raw lexer. */
    struct sh_raw_lex_context raw_ctx;

    /** The current state of the lexer. */
    enum sh_lex_state state;

    /** Whether the (first character of the) next token should be escaped. */
    bool escape;

    /** Keeps track of the start quote type (' or ") when in a quoted string */
    struct sh_raw_token start_quote;

    /** Buffer for concatenating strings and word sections. */
    size_t catbuf_capacity;
    size_t catbuf_len;
    char *catbuf;
};

/** Represents the result of a call to `lex()`. */
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
 * Initialises a lex context for the given input string.
 *
 * @param input the input string
 * @param ctx_out a pointer to the context to initialise
 */
void init_lex_context(struct sh_lex_context *ctx_out, char const *input);

/**
 * Lexes an input string specified by the context into a sequence of tokens.
 *
 * This function is reentrant and should be called with a lex context
 * initialised by `init_lex_context()`. Each lex should have this function
 * called multiple times with the same context.
 *
 * Output tokens are stored in the lex context.
 *
 * The behaviour of this function filters out whitespace, combines quotes and
 * text into words and expands globs.
 *
 * For the return value, see `enum sh_lex_result`.
 *
 * @param ctx the lex context
 * @param token_in a pointer to write the token to
 *
 * @return the result of the current iteration
 */
enum sh_lex_result lex(struct sh_lex_context *ctx);

/**
 * Destroys the given lex context.
 *
 * @param ctx the lex context to destroy
 */
void destroy_lex_context(struct sh_lex_context *ctx);

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
