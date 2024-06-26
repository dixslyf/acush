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
 * A special token is any of the following: & ; | < > 2> ! ' " * ? [.
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

void init_lex_lossless_context(
    char const *input,
    struct sh_lex_lossless_context *ctx_out
) {
    ctx_out->cp = input;
}

enum sh_lex_result
lex_lossless(struct sh_lex_lossless_context *ctx, struct sh_token *token_out) {
    // If the current character is the null character, then the lex has
    // finished.
    if (*ctx->cp == '\0') {
        *token_out = (struct sh_token) {
            .type = SH_TOKEN_END,
            .text = "\0",
        };
        return SH_LEX_END;
    }

    // Try lexing a special token.
    struct sh_token token;
    if (lex_special(ctx->cp, &token)) {
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

    // At this point, the token is either whitespace or a word.
    // (Technically, if it is not whitespace, "word" isn't the right term
    // since words can be like `"foo"'bar'baz`, but that is not important.)

    // Keep track of the start of the token.
    char const *text_start = ctx->cp;

    // Handling whitespace and handilng words are similar, so we can share
    // most of their code.
    enum sh_token_type token_type;
    bool (*cont_predicate)(char const *);
    if (is_ws_delimiter(ctx->cp)) {
        token_type = SH_TOKEN_WHITESPACE;
        cont_predicate = is_ws_delimiter;
    } else {
        token_type = SH_TOKEN_WORD;
        cont_predicate = is_word_char;
    }

    // Find the end of the token.
    do {
        ctx->cp++;
    } while (cont_predicate(ctx->cp));

    size_t text_len = ctx->cp - text_start;

    // Allocate memory for the token's text.
    // `+ 1` for terminating null character.
    char *text = malloc(sizeof(char) * text_len + 1);
    if (text == NULL) {
        return SH_LEX_MEMORY_ERROR;
    }

    // Copy the corresponding substring into the allocated buffer.
    strncpy(text, text_start, text_len);
    text[text_len] = '\0';

    // No need to increment `ctx->cp` since it should already be
    // pointing to the next unseen character.

    *token_out = (struct sh_token) {
        .type = token_type,
        .text = text,
    };

    return SH_LEX_ONGOING;
}

void destroy_token(struct sh_token *token) {
    // Only the `text` for `SH_TOKEN_WORDS` and `SH_TOKEN_WHITESPACE` are
    // dynamically allocated. Other token types use static allocation.
    if (token->type == SH_TOKEN_WORD || token->type == SH_TOKEN_WHITESPACE) {
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

    default:
        return false;
    }

    *token_out = token;

    return true;
}

bool is_ws_delimiter(char const *cp) {
    return strchr(WHITESPACE_DELIMITERS, *cp);
}

bool is_quote(char const *cp) { return *cp == '"' || *cp == '\''; }

bool is_special(char const *cp) {
    return strchr("&;|<>!'\"*?[", *cp) || (*cp == '2' && *(cp + 1) == '>');
}

bool is_word_boundary(char const *cp) {
    return is_ws_delimiter(cp) || is_special(cp) || *cp == '\0';
}

bool is_word_char(char const *cp) { return !is_word_boundary(cp); }
