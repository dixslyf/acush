#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "raw_lex.h"

#define WHITESPACE_DELIMITERS " \n\t\f\r\v"

/**
 * Attempts to lex the given character pointer into a special raw token.
 * `true` is returned and a token corresponding to `*cp` is written to
 * `token_out` if `cp` indeed represents a special token. Otherwise, `false` is
 * returned.
 *
 * See `is_special()` for what counts as a special token.
 *
 * @param cp the character pointer to try lexing into a special token
 * @param token_out a pointer to write the output token to if successful
 *
 * @return `true` if `cp` was successfully parsed into a special token;
 * otherwise, `false`
 */
bool lex_special(char const *cp, struct sh_raw_token *token_out);

/**
 * Attempts to lex the given character pointer into a whitespace token.
 * `true` is returned and a token corresponding to `*cp` is written to
 * `token_out` if `cp` indeed represents a whitespace token. Otherwise, `false`
 * is returned.
 *
 * See `is_ws_delimiter()` for what counts as a whitespace token.
 *
 * @param cp the character pointer to try lexing into a whitespace token
 * @param token_out a pointer to write the output token to if successful
 *
 * @return `true` if `cp` was successfully parsed into a whitespace token;
 * otherwise, `false`
 */
bool lex_whitespace(char const *cp, struct sh_raw_token *token_out);

/**
 * Returns `true` if `*cp` is a whitespace delimiter. Otherwise, returns
 * `false`.
 *
 * Whitespace delimiters are those in `WHITESPACE_DELIMITERS`.
 *
 * @param cp the character pointer to check
 * @return `true` if `*cp` is a whitespace delimiter; otherwise, `false`
 */
bool is_ws_delimiter(char const *cp);

/**
 * Returns `true` if `cp` represents a special token.
 *
 * A special token is any of the following: & ; | < > 2> ! ' " * ? [ \.
 *
 * @param cp the character pointer to check
 * @return `true` if `*cp` represents a special token; otherwise, `false`
 */
bool is_special(char const *cp);

/**
 * Returns `true` if `cp` is a text boundary. Otherwise, returns `false`.
 *
 * `cp` is a text boundary if any of the following evaluate to `true`:
 *   - `is_ws_delimiter(cp)`
 *   - `is_special(cp)`
 *   - `*cp == '\0'`
 *
 * @param cp the character pointer to check
 * @return `true` if `cp` represents a text boundary; otherwise, `false`
 */
bool is_text_boundary(char const *cp);

/**
 * Returns `true` if `cp` is not a text boundary. Otherwise, returns `false`.
 *
 * See `is_text_boundary()`.
 *
 * @param cp the character pointer to check
 * @return `true` if `cp` is not a text boundary; otherwise, `false`
 */
bool is_text_char(char const *cp);

void init_raw_lex_context(
    struct sh_raw_lex_context *ctx_out,
    char const *input
) {
    ctx_out->cp = input;
    ctx_out->finished = false;
}

enum sh_raw_lex_result
raw_lex(struct sh_raw_lex_context *ctx, struct sh_raw_token *token_out) {
    if (ctx->finished) {
        return SH_RAW_LEX_END;
    }

    // If the current character is the null character, we still need to send the
    // terminating end token.
    if (*ctx->cp == '\0') {
        *token_out = (struct sh_raw_token) {
            .type = SH_RAW_TOKEN_END,
            .text = "\0",
        };

        ctx->finished = true;

        return SH_RAW_LEX_ONGOING;
    }

    // Try lexing a special or whitespace token.
    struct sh_raw_token token;
    if (lex_special(ctx->cp, &token) || lex_whitespace(ctx->cp, &token)) {
        // Increment the pointer.
        if (token.type == SH_RAW_TOKEN_2_ANGLE_BRACKET_R) {
            // Special case for `2>` since it has two characters.
            ctx->cp += 2;
        } else {
            ctx->cp++;
        }

        *token_out = token;
        return SH_RAW_LEX_ONGOING;
    }

    // At this point, the token is a text token.

    // Keep track of the start of the token.
    char const *text_start = ctx->cp;

    // Find the end of the token.
    do {
        ctx->cp++;
    } while (is_text_char(ctx->cp));

    // Allocate memory for the token's text.
    size_t text_len = ctx->cp - text_start;
    char *text = malloc(
        sizeof(char) * text_len + 1
    ); // `+ 1` for terminating null character.
    if (text == NULL) {
        return SH_RAW_LEX_MEMORY_ERROR;
    }

    // Copy the corresponding substring into the allocated buffer.
    strncpy(text, text_start, text_len);
    text[text_len] = '\0';

    // No need to increment `ctx->cp` since it should already be
    // pointing to the next unseen character.

    *token_out = (struct sh_raw_token) {
        .type = SH_RAW_TOKEN_TEXT,
        .text = text,
    };

    return SH_RAW_LEX_ONGOING;
}

void destroy_raw_token(struct sh_raw_token *token) {
    // Only the `text` for `SH_RAW_TOKEN_TEXT` is dynamically allocated. Other
    // token types use static allocation.
    if (token->type == SH_RAW_TOKEN_TEXT) {
        // NOTE: This is a safe cast since we are no longer using it. Strangely,
        // `free()` takes in a non-const pointer. Linus Torvalds (creator of
        // Linux) seems to agree that `free()` shouldn't take in a non-const
        // pointer.
        free((char *) token->text);
        token->text = NULL;
    }
}

bool lex_special(char const *cp, struct sh_raw_token *token_out) {
    static char const CHARS[] = "&;!|<>2'\"*?[\\";
    static enum sh_raw_token_type const TOKEN_TYPES[] = {
        SH_RAW_TOKEN_AMP,
        SH_RAW_TOKEN_SEMICOLON,
        SH_RAW_TOKEN_EXCLAM,
        SH_RAW_TOKEN_PIPE,
        SH_RAW_TOKEN_ANGLE_BRACKET_L,
        SH_RAW_TOKEN_ANGLE_BRACKET_R,
        SH_RAW_TOKEN_2_ANGLE_BRACKET_R,
        SH_RAW_TOKEN_SINGLE_QUOTE,
        SH_RAW_TOKEN_DOUBLE_QUOTE,
        SH_RAW_TOKEN_ASTERISK,
        SH_RAW_TOKEN_QUESTION,
        SH_RAW_TOKEN_SQUARE_BRACKET_L,
        SH_RAW_TOKEN_BACKSLASH,
    };
    static char const *const STRINGS[] =
        {"&", ";", "!", "|", "<", ">", "2>", "'", "\"", "*", "?", "[", "\\"};

    struct sh_raw_token token;

    // Check if the character is not a special character.
    if (!is_special(cp)) {
        return false;
    }

    // At this point, the character must represent a special token.
    char *c = strchr(CHARS, *cp);
    assert(c != NULL);

    // Get the index of the character within `CHARS` so that we can index into
    // `STRINGS` to get the corresponding string.
    size_t idx = c - CHARS;
    token.type = TOKEN_TYPES[idx];
    char const *str = STRINGS[idx];
    token.text = str;

    *token_out = token;
    return true;
}

bool lex_whitespace(char const *cp, struct sh_raw_token *token_out) {
    // For mapping each whitespace character to a string.
    static char const CHARS[] = " \n\t\f\r\v";
    static char const *STRINGS[] = {" ", "\n", "\t", "\f", "\r", "\v"};

    // Check if the character is not a whitespace character.
    if (!is_ws_delimiter(cp)) {
        return false;
    }

    // At this point, the character must be a whitespace character.
    char *c = strchr(CHARS, *cp);
    assert(c != NULL);

    // Get the index of the character within `CHARS` so that we can index into
    // `STRINGS` to get the corresponding string.
    size_t idx = c - CHARS;
    char const *str = STRINGS[idx];

    struct sh_raw_token token;
    token.type = SH_RAW_TOKEN_WHITESPACE;
    token.text = str;

    *token_out = token;
    return true;
}

bool is_ws_delimiter(char const *cp) {
    return strchr(WHITESPACE_DELIMITERS, *cp);
}

bool is_quote(char const *cp) { return *cp == '"' || *cp == '\''; }

bool is_special(char const *cp) {
    return strchr("&;|<>!'\"*?[\\", *cp) || (*cp == '2' && *(cp + 1) == '>');
}

bool is_text_boundary(char const *cp) {
    return is_ws_delimiter(cp) || is_special(cp) || *cp == '\0';
}

bool is_text_char(char const *cp) { return !is_text_boundary(cp); }
