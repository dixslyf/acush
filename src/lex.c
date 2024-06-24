#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"

#define WHITESPACE_DELIMITERS " \n\t\f\r\v"

/**
 * Attempts to lex the given character pointer into a simple special token.
 * `0` is returned and a token corresponding to `*cp` is written to `token_out`
 * if `cp` indeed represents a simple special token. Otherwise, `-1` is
 * returned.
 *
 * See `is_simple_special()` for what counts as a simple special token.
 *
 * @param cp the character pointer to try lexing into a simple special token
 * @param token_out a pointer to write the output token to
 *
 * @return `0` if `cp` was successfully parsed into a simple special token;
 * otherwise, `-1`
 */
int lex_simple_special(char const *cp, struct sh_token *token_out);

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
 * Returns `true` if `*cp` is a quote (' or "). Otherwise, returns `false`.
 *
 * @param cp the character pointer to check
 * @return `true` if `*cp` is a quote; otherwise, `false`
 */
bool is_quote(char const *cp);

/**
 * Returns `true` if `cp` represents a simple special token.
 *
 * A simple special token is one of the following: & ; | < > 2> ! ' ".
 *
 * @param cp the character pointer to check
 * @return `true` if `*cp` represents a simple special token; otherwise, `false`
 */
bool is_simple_special(char const *cp);

/**
 * Returns `true` if `*cp` is a word boundary. Otherwise, returns `false`.
 *
 * `*cp` is a word boundary if any of the following evaluate to `true`:
 *   - `is_ws_delimiter(cp)`
 *   - `is_simple_special(cp)`
 *   - `*cp == '\0'`
 *
 * @param cp the character pointer to check
 * @return `true` if `*cp` represents a word boundary; otherwise, `false`
 */
bool is_word_boundary(char const *cp);

/** Represents a possible state of the lexing process. */
enum sh_state {
    /** Indicates that the lexer is currently not in a word. */
    SH_STATE_DULL,

    /** Indicates that the lexer is in a quoted section of a word. */
    SH_STATE_QUOTED,

    /** Indicates that the lexer is in an unquoted section of a word. */
    SH_STATE_UNQUOTED
};

struct sh_lex_context {
    // Lexing is implemented as a finite state machine (FSM).
    // `state` keeps track of the FSM's state.
    enum sh_state state;

    // Store the original input.
    char const *input;

    // Keeps track of the current character being processed in the input
    // string.
    char const *cp;

    // Keeps track of the start quote when inside a quoted section of a word.
    char const *quote_start;

    // Set up a buffer for string concatenation for when we are in a word.
    size_t catbuf_capacity;
    size_t catbuf_idx;
    char *catbuf;
};

struct sh_lex_context *init_lex_context(char const *input) {
    struct sh_lex_context ctx;

    // We initialise the state of the FSM based on the first character of the
    // input.
    if (is_word_boundary(input)) {
        ctx.state = SH_STATE_DULL;
    } else if (is_quote(input)) {
        ctx.state = SH_STATE_QUOTED;
    } else {
        ctx.state = SH_STATE_UNQUOTED;
    }

    ctx.input = input;
    ctx.cp = input;

    ctx.quote_start = NULL;

    // The worst case scenario is that the entire input is a single word.
    // Hence, the buffer must have a size at least equal to the length of the
    // input string (including the null byte).
    ctx.catbuf_capacity = strlen(input) + 1;
    ctx.catbuf_idx = 0;
    ctx.catbuf = malloc(ctx.catbuf_capacity);
    if (ctx.catbuf == NULL) {
        return NULL;
    }

    // Unfortunately, we do have to dynamically allocate memory since `struct
    // sh_lex_context` is opaque to callers. Small tradeoff for encapsulation.
    struct sh_lex_context *ctx_out = malloc(sizeof(struct sh_lex_context));
    if (ctx_out == NULL) {
        free(ctx.catbuf);
        return NULL;
    }

    *ctx_out = ctx;
    return ctx_out;
}

void destroy_lex_context(struct sh_lex_context *ctx) {
    free(ctx->catbuf);
    free(ctx);
}

enum sh_lex_result lex(struct sh_lex_context *ctx, struct sh_token *token_out) {
    struct sh_token token;
    for (; *ctx->cp != '\0'; ctx->cp++) {
        switch (ctx->state) {
        case SH_STATE_DULL:
            // Peek at the next character to see if we need to change
            // state.
            if (is_quote(ctx->cp + 1)) {
                // Start of quoted section of a word.
                ctx->state = SH_STATE_QUOTED;
            } else if (!is_word_boundary(ctx->cp + 1)) {
                // Start of unquoted section of a word.
                ctx->state = SH_STATE_UNQUOTED;
            }

            // Try lexing simple special tokens: & ; ! | < > 2>.
            // However, if we just lexed 2>, then don't try lexing the >.
            if (!(ctx->cp != ctx->input && *(ctx->cp - 1) == '2'
                  && *(ctx->cp) == '>')
                && lex_simple_special(ctx->cp, &token) == 0)
            {
                *token_out = token;
                ctx->cp++; // Need to manually increment since we
                           // are skipping the loop's increment.
                return SH_LEX_ONGOING;
            }

            // At this point, the character is either one of the
            // whitespace delimiters or the > from 2>. In either case,
            // we skip them.
            break;
        case SH_STATE_QUOTED:
            // The first time we enter this state, `quote_start` should
            // be `NULL` and the character should be a quote.
            if (ctx->quote_start == NULL) {
                assert(is_quote(ctx->cp));
                ctx->quote_start = ctx->cp;
                // We don't want to have the quote in the buffer,
                // so we don't add it.
                continue;
            }

            // Handle quote escape sequence.
            if (*ctx->cp == '\\' && *(ctx->cp + 1) == *ctx->quote_start) {
                // Skip the backslash.
                ctx->cp++;

                // Add the escaped quote to the buffer.
                ctx->catbuf[ctx->catbuf_idx] = *ctx->cp;
                ctx->catbuf_idx++;
                continue;
            }

            // Still in the quoted word, so add the character to the
            // buffer.
            if (*ctx->cp != *ctx->quote_start) {
                ctx->catbuf[ctx->catbuf_idx] = *ctx->cp;
                ctx->catbuf_idx++;
                continue;
            }

            // At this point, `*c` is the closing quote. However, to
            // decide which state to switch to, we need to check if
            // we're still in a word by peeking at the next character.

            // Reset `quote_start` for the next quoted section.
            ctx->quote_start = NULL;

            // The next character is another quote, indicating the
            // start of another quoted word.
            if (is_quote(ctx->cp + 1)) {
                // No need to change state since we're in a quoted
                // word again.
                continue;
            }

            // If the current character is the last in the current
            // word, then we are done with the current word and can now
            // create the token for it.
            if (is_word_boundary(ctx->cp + 1)) {
                ctx->state = SH_STATE_DULL;

                // However, if we have an empty pair of quotes ("",
                // ''), then don't create the token.
                if (ctx->catbuf_idx != 0) {
                    token.type = SH_TOKEN_WORD;

                    // We need to dynamically allocate memory
                    // this time.
                    token.text = malloc(ctx->catbuf_idx + 1);
                    strncpy(token.text, ctx->catbuf, ctx->catbuf_idx);
                    token.text[ctx->catbuf_idx] = '\0';

                    *token_out = token;

                    ctx->catbuf_idx = 0; // Reset for future words.

                    ctx->cp++; // Need to manually increment
                               // since we are skipping the
                               // loop's increment.

                    return SH_LEX_ONGOING;
                }
            }

            // If none of the above checks are true, then we must still
            // be within a word, but now we are at the start of an
            // unquoted section of the word. E.g., "foo"bar (the next
            // character would be 'b').
            ctx->state = SH_STATE_UNQUOTED;
            break;
        case SH_STATE_UNQUOTED:
            // Handle escape sequences.
            if (*ctx->cp == '\\') {
                // Skip the backslash. Now, `ctx->cp` points to the
                // escaped character.
                ctx->cp++;
            }

            // Add the character to the buffer.
            ctx->catbuf[ctx->catbuf_idx] = *ctx->cp;
            ctx->catbuf_idx++;

            // Now, to see if we need to change state, we peek at the
            // next character.

            // If the next character is a quote, then we will still be
            // in the same word, but at the start of a quoted string.
            if (is_quote(ctx->cp + 1)) {
                ctx->state = SH_STATE_QUOTED;
                continue;
            }

            // If the current character is the last in the word, then
            // we create the token.
            if (is_word_boundary(ctx->cp + 1)) {
                token.type = SH_TOKEN_WORD;

                // We need to dynamically allocate memory this
                // time.
                token.text = malloc(ctx->catbuf_idx + 1);
                strncpy(token.text, ctx->catbuf, ctx->catbuf_idx);
                token.text[ctx->catbuf_idx] = '\0';

                *token_out = token;

                ctx->state = SH_STATE_DULL;
                ctx->catbuf_idx = 0; // Reset for future words.
                ctx->cp++;           // Need to manually increment since we
                                     // are skipping the loop's increment.
                return SH_LEX_ONGOING;
            }
        }
    }

    if (ctx->state == SH_STATE_QUOTED) {
        return SH_LEX_UNTERMINATED_QUOTE;
    }

    return SH_LEX_END;
}

void destroy_token(struct sh_token *token) {
    // Only the `text` for `SH_TOKEN_WORDS` are dynamically allocated.
    // Other token types use static allocation.
    if (token->type == SH_TOKEN_WORD) {
        free(token->text);
        token->text = NULL;
    }
}

int lex_simple_special(char const *cp, struct sh_token *token_out) {
    struct sh_token token;
    switch (*cp) {
    case '&':
        token.type = SH_TOKEN_AMP;
        token.text = "&";
        break;

    case ';':
        token.type = SH_TOKEN_SEMICOLON;
        token.text = ";";
        break;

    case '!':
        token.type = SH_TOKEN_EXCLAM;
        token.text = "!";
        break;

    case '|':
        token.type = SH_TOKEN_PIPE;
        token.text = "|";
        break;

    case '<':
        token.type = SH_TOKEN_ANGLE_BRACKET_L;
        token.text = "<";
        break;

    case '>':
        token.type = SH_TOKEN_ANGLE_BRACKET_R;
        token.text = ">";
        break;

    case '2':
        if (*(cp + 1) == '>') {
            token.type = SH_TOKEN_2_ANGLE_BRACKET_R;
            token.text = "2>";
            break;
        }
        return -1;

    default:
        return -1;
    }

    *token_out = token;

    return 0;
}

bool is_ws_delimiter(char const *cp) {
    return strchr(WHITESPACE_DELIMITERS, *cp);
}

bool is_quote(char const *cp) { return *cp == '"' || *cp == '\''; }

bool is_simple_special(char const *cp) {
    return strchr("&;|<>!", *cp) || (*cp == '2' && *(cp + 1) == '>');
}

bool is_word_boundary(char const *cp) {
    return is_ws_delimiter(cp) || is_simple_special(cp) || *cp == '\0';
}
