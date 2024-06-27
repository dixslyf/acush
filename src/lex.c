#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"

#define WHITESPACE_DELIMITERS " \n\t\f\r\v"

/**
 * Attempts to lex the given character pointer into a special token.
 * `0` is returned and a token corresponding to `*cp` is written to `token_out`
 * if `cp` indeed represents a special token. Otherwise, `-1` is returned.
 *
 * See `is_special()` for what counts as a special token.
 *
 * @param cp the character pointer to try lexing into a special token
 * @param token_out a pointer to write the output token to
 *
 * @return `true` if `cp` was successfully parsed into a special token;
 * otherwise, `false`
 */
bool lex_special(char const *cp, struct sh_token *token_out);

bool lex_whitespace(char const *cp, struct sh_token *token_out);

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
 * A special token is any of the following: & ; | < > 2 ! ' " * ? [ \.
 *
 * @param cp the character pointer to check
 * @return `true` if `*cp` represents a special token; otherwise, `false`
 */
bool is_special(char const *cp);

/**
 * Returns `true` if `cp` is a word boundary. Otherwise, returns `false`.
 *
 * `cp` is a word boundary if any of the following evaluate to `true`:
 *   - `is_ws_delimiter(cp)`
 *   - `is_special(cp)`
 *   - `*cp == '\0'`
 *
 * @param cp the character pointer to check
 * @return `true` if `cp` represents a word boundary; otherwise, `false`
 */
bool is_word_boundary(char const *cp);

/**
 * Returns `true` if `cp` is not a word boundary. Otherwise, returns `false`.
 *
 * See `is_word_boundary()`.
 *
 * @param cp the character pointer to check
 * @return `true` if `cp` is not a word boundary; otherwise, `false`
 */
bool is_word_char(char const *cp);

int append_to_catbuf(
    struct sh_lex_refine_context *ctx,
    char const *text,
    size_t text_len
);

int append_to_tokbuf(struct sh_lex_refine_context *ctx, struct sh_token token);

void init_lex_lossless_context(
    char const *input,
    struct sh_lex_lossless_context *ctx_out
) {
    ctx_out->cp = input;
    ctx_out->finished = false;
}

void init_lex_refine_context(struct sh_lex_refine_context *ctx_out) {
    *ctx_out = (struct sh_lex_refine_context) {
        .state = SH_LEX_REFINE_DULL,
        .escape = false,

        .catbuf_capacity = 0,
        .catbuf_len = 0,
        .catbuf = NULL,

        .tokbuf_capacity = 0,
        .tokbuf_len = 0,
        .tokbuf = NULL,
    };
}

enum sh_lex_result
lex_lossless(struct sh_lex_lossless_context *ctx, struct sh_token *token_out) {
    if (ctx->finished) {
        return SH_LEX_END;
    }

    // If the current character is the null character, we still need to send the
    // terminating end token.
    if (*ctx->cp == '\0') {
        *token_out = (struct sh_token) {
            .type = SH_TOKEN_END,
            .text = "\0",
        };

        ctx->finished = true;

        return SH_LEX_ONGOING;
    }

    // Try lexing a special or whitespace token.
    struct sh_token token;
    if (lex_special(ctx->cp, &token) || lex_whitespace(ctx->cp, &token)) {
        // Increment the pointer.
        if (token.type == SH_TOKEN_2_ANGLE_BRACKET_R) {
            // Special case for `2>` since it has two characters.
            ctx->cp += 2;
        } else {
            ctx->cp++;
        }

        *token_out = token;
        return SH_LEX_ONGOING;
    }

    // At this point, the token is a word. (Technically, "word" isn't the right
    // term since words can be like `"foo"'bar'baz`, but that is not important.)

    // Keep track of the start of the token.
    char const *text_start = ctx->cp;

    // Find the end of the token.
    do {
        ctx->cp++;
    } while (is_word_char(ctx->cp));

    // Allocate memory for the token's text.
    size_t text_len = ctx->cp - text_start;
    char *text = malloc(
        sizeof(char) * text_len + 1
    ); // `+ 1` for terminating null character.
    if (text == NULL) {
        return SH_LEX_MEMORY_ERROR;
    }

    // Copy the corresponding substring into the allocated buffer.
    strncpy(text, text_start, text_len);
    text[text_len] = '\0';

    // No need to increment `ctx->cp` since it should already be
    // pointing to the next unseen character.

    *token_out = (struct sh_token) {
        .type = SH_TOKEN_WORD,
        .text = text,
    };

    return SH_LEX_ONGOING;
}

enum sh_lex_result
lex_refine(struct sh_lex_refine_context *ctx, struct sh_token const *token_in) {
    // Determine which state to change to.
    enum sh_lex_refine_state old_state = ctx->state;
    if (ctx->escape) {
        // Don't do any state transitions if we're escaping the current token.
        assert(
            old_state == SH_LEX_REFINE_WORD_QUOTED
            || old_state == SH_LEX_REFINE_WORD_UNQUOTED
        );
    } else if (old_state == SH_LEX_REFINE_WORD_QUOTED
        && token_in->type == ctx->start_quote.type)
    {
        ctx->state = SH_LEX_REFINE_WORD_QUOTED_END;
    } else if (old_state == SH_LEX_REFINE_WORD_QUOTED && token_in->type == SH_TOKEN_END)
    {
        return SH_LEX_UNTERMINATED_QUOTE;
    } else if (old_state == SH_LEX_REFINE_WORD_QUOTED) {
        // No state transition — the only way to leave the quoted state is to
        // see the end quote.
    } else if (token_in->type == SH_TOKEN_WORD || token_in->type == SH_TOKEN_BACKSLASH || token_in->type == SH_TOKEN_ASTERISK || token_in->type == SH_TOKEN_QUESTION || token_in->type == SH_TOKEN_SQUARE_BRACKET_L)
    {
        ctx->state = SH_LEX_REFINE_WORD_UNQUOTED;
    } else if (token_in->type == SH_TOKEN_DOUBLE_QUOTE || token_in->type == SH_TOKEN_SINGLE_QUOTE)
    {
        ctx->state = SH_LEX_REFINE_WORD_QUOTED;
        ctx->start_quote = *token_in;

        // We don't actually need to do anything with the opening quote.
        return SH_LEX_ONGOING;
    } else {
        ctx->state = SH_LEX_REFINE_DULL;
    }

    // If we exited a word, then write the token.
    if ((old_state == SH_LEX_REFINE_WORD_QUOTED_END
         || old_state == SH_LEX_REFINE_WORD_UNQUOTED)
        && (ctx->state == SH_LEX_REFINE_DULL || token_in->type == SH_TOKEN_END))
    {
        struct sh_token token = (struct sh_token) {
            .type = SH_TOKEN_WORD,
            .text = ctx->catbuf,
        };

        if (append_to_tokbuf(ctx, token) < 0) {
            return SH_LEX_MEMORY_ERROR;
        }

        ctx->catbuf_capacity = 0;
        ctx->catbuf_len = 0;
        ctx->catbuf = NULL;
    }

    switch (ctx->state) {
    case SH_LEX_REFINE_DULL: {
        // Check if the token is a whitespace token. Whitespace tokens are
        // thrown away for the dull state.
        if (token_in->type == SH_TOKEN_WHITESPACE) {
            break;
        }

        // At this point, the token must be one of the other special characters.

        struct sh_token token = (struct sh_token) {
            .type = token_in->type,
            .text = token_in->text, // This is guaranteed to be statically
                                    // allocated, so no copying is necessary.
        };

        if (append_to_tokbuf(ctx, token) < 0) {
            return SH_LEX_MEMORY_ERROR;
        }

        break;
    }

    case SH_LEX_REFINE_WORD_QUOTED:
    case SH_LEX_REFINE_WORD_UNQUOTED: {
        // Handle escaped characters.
        if (ctx->escape) {
            if (append_to_catbuf(ctx, token_in->text, strlen(token_in->text))
                < 0)
            {
                return SH_LEX_MEMORY_ERROR;
            }
            ctx->escape = false;
            break;
        }

        // If the token is a backslash, then we need to escape the next token.
        if (token_in->type == SH_TOKEN_BACKSLASH) {
            // Backslash followed by any character will make that character be
            // taken literally, so we can just add a backslash unconditionally.
            // Keep in mind that we are building a string to pass to `glob()`.
            if (append_to_catbuf(ctx, "\\", 1) < 0) {
                return SH_LEX_MEMORY_ERROR;
            }
            ctx->escape = true;
            break;
        }

        // Otherwise, we just add the token's text as is.
        if (append_to_catbuf(ctx, token_in->text, strlen(token_in->text))) {
            return SH_LEX_MEMORY_ERROR;
        }
        break;
    }

    case SH_LEX_REFINE_WORD_QUOTED_END:
        break;
    }

    return SH_LEX_ONGOING;
}

void destroy_token(struct sh_token *token) {
    // Only the `text` for `SH_TOKEN_WORDS` is dynamically allocated. Other
    // token types use static allocation.
    if (token->type == SH_TOKEN_WORD) {
        free(token->text);
        token->text = NULL;
    }
}

bool lex_special(char const *cp, struct sh_token *token_out) {
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
        return false;

    case '\'':
        token.type = SH_TOKEN_SINGLE_QUOTE;
        token.text = "'";
        break;

    case '"':
        token.type = SH_TOKEN_DOUBLE_QUOTE;
        token.text = "\"";
        break;

    case '*':
        token.type = SH_TOKEN_ASTERISK;
        token.text = "*";
        break;

    case '?':
        token.type = SH_TOKEN_QUESTION;
        token.text = "?";
        break;

    case '[':
        token.type = SH_TOKEN_SQUARE_BRACKET_L;
        token.text = "[";
        break;

    case '\\':
        token.type = SH_TOKEN_BACKSLASH;
        token.text = "\\";
        break;

    default:
        assert(!is_special(cp));
        return false;
    }

    *token_out = token;
    return true;
}

bool lex_whitespace(char const *cp, struct sh_token *token_out) {
    struct sh_token token;
    switch (*cp) {
    case ' ':
        token.type = SH_TOKEN_WHITESPACE;
        token.text = " ";
        break;

    case '\n':
        token.type = SH_TOKEN_WHITESPACE;
        token.text = "\n";
        break;

    case '\t':
        token.type = SH_TOKEN_WHITESPACE;
        token.text = "\t";
        break;

    case '\f':
        token.type = SH_TOKEN_WHITESPACE;
        token.text = "\f";
        break;

    case '\r':
        token.type = SH_TOKEN_WHITESPACE;
        token.text = "\r";
        break;

    case '\v':
        token.type = SH_TOKEN_WHITESPACE;
        token.text = "\v";
        break;

    default:
        assert(!is_ws_delimiter(cp));
        return false;
    }

    *token_out = token;
    return true;
}

bool is_ws_delimiter(char const *cp) {
    return strchr(WHITESPACE_DELIMITERS, *cp);
}

bool is_quote(char const *cp) { return *cp == '"' || *cp == '\''; }

bool is_special(char const *cp) { return strchr("&;|<>2!'\"*?[\\", *cp); }

bool is_word_boundary(char const *cp) {
    return is_ws_delimiter(cp) || is_special(cp) || *cp == '\0';
}

bool is_word_char(char const *cp) { return !is_word_boundary(cp); }

int append_to_catbuf(
    struct sh_lex_refine_context *ctx,
    char const *text,
    size_t text_len
) {
    // Grow the buffer if needed.
    // `+ 1` for null character.
    if (ctx->catbuf_len + text_len + 1 > ctx->catbuf_capacity) {
        size_t new_capacity = ctx->catbuf_capacity + text_len * 2 + 1;
        char *tmp = realloc(ctx->catbuf, sizeof(char) * new_capacity);
        if (tmp == NULL) {
            return -1;
        }

        ctx->catbuf_capacity = new_capacity;
        ctx->catbuf = tmp;
    }

    // Append the text to the buffer.
    strncpy(ctx->catbuf + ctx->catbuf_len, text, text_len);
    ctx->catbuf_len += text_len;
    ctx->catbuf[ctx->catbuf_len] = '\0';
    return 0;
}

int append_to_tokbuf(struct sh_lex_refine_context *ctx, struct sh_token token) {
    // Grow the buffer if needed.
    // `+ 1` for null character.
    if (ctx->tokbuf_len + 1 > ctx->tokbuf_capacity) {
        size_t new_capacity = ctx->tokbuf_capacity == 0
                                  ? 4
                                  : ctx->tokbuf_capacity * 2;

        struct sh_token *tmp = realloc(
            ctx->tokbuf,
            sizeof(struct sh_token) * new_capacity
        );

        if (tmp == NULL) {
            return -1;
        }

        ctx->tokbuf_capacity = new_capacity;
        ctx->tokbuf = tmp;
    }

    // Append the token to the buffer.
    ctx->tokbuf[ctx->tokbuf_len] = token;
    ctx->tokbuf_len++;

    return 0;
}
