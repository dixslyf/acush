#include <assert.h>
#include <glob.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "raw_lex.h"

/**
 * Returns the corresponding token type from the given raw token type.
 *
 * If there is no corresponding token type, an assertion failure will be raised!
 *
 * @param raw_token_type the raw token type to get the corresponding token type
 * for
 * @return the corresonding token type
 */
enum sh_token_type
token_type_from_raw_token_type(enum sh_raw_token_type raw_token_type);

/** Represents the result of appending to a dynamically allocated buffer. */
enum sh_append_result {
    SH_APPEND_SUCCESS,
    SH_APPEND_MEMORY_ERROR,
};

/**
 * Appends the given text content to the concatenation buffer of the given
 * context.
 *
 * @param ctx the lex context whose concatenation buffer is to be appended to
 * @param text the text content to append to the concatenation buffer
 * @param the length of the text content (exclusive of the null character)
 *
 * @return `SH_APPEND_SUCCESS` if successful or `SH_APPEND_MEMORY_ERROR` if
 * memory allocation failed
 */
enum sh_append_result
append_to_catbuf(struct sh_lex_context *ctx, char const *text, size_t text_len);

/**
 * Appends the given token to the token buffer of the given context.
 *
 * @param ctx the lex context whose token buffer is to be appended to
 * @param text the token to append to the token buffer
 *
 * @return `SH_APPEND_SUCCESS` if successful or `SH_APPEND_MEMORY_ERROR` if
 * memory allocation failed
 */
enum sh_append_result
append_to_tokbuf(struct sh_lex_context *ctx, struct sh_token token);

/**
 * Checks if the given raw token type indicates the start of an unquoted section
 * of a word.
 *
 * A raw token type indicates as such if it is one of the following:
 * `SH_RAW_TOKEN_TEXT`, `SH_RAW_TOKEN_BACKSLASH`, `SH_RAW_TOKEN_ASTERISK`,
 * `SH_RAW_TOKEN_QUESTION` or `SH_RAW_TOKEN_SQUARE_BRACKET_L`.
 *
 * @param raw_tok_type the raw token type to check
 * @return `true` if the raw token type indicates the start of an unquoted
 * section of a word and `false` otherwise
 */
bool is_unquoted_section_marker(enum sh_raw_token_type raw_tok_type);

/** Represents the result of ending a word token. */
enum sh_end_word_result {
    SH_END_WORD_SUCCESS,
    SH_END_WORD_MEMORY_ERROR,
    SH_END_WORD_GLOB_ABORTED,
};

/**
 * Ends the current word token for the given context.
 *
 * This function should only be called after the completion of lexing a word
 * token!
 *
 * This function attempts to expand the current contents of the concatenation
 * buffer by treating the contents as a glob pattern. If the expansion is
 * successful, all resulting paths are converted into word tokens and added to
 * the token buffer of the context. If the expansion yields no matching results,
 * then the concatenation buffer is converted to a word token after removing the
 * escaping backslashes (since those backslashes are for escaping metacharacters
 * before passing to `glob()`).
 *
 * @param ctx the lex context
 * @return the result of attempting to end the word
 */
enum sh_end_word_result end_word(struct sh_lex_context *ctx);

void init_lex_context(struct sh_lex_context *ctx_out, char const *input) {
    // Initialise the context.
    *ctx_out = (struct sh_lex_context) {
        .tokbuf_capacity = 0,
        .tokbuf_len = 0,
        .tokbuf = NULL,

        .state = SH_LEX_STATE_DULL,
        .escape = false,

        .catbuf_capacity = 0,
        .catbuf_len = 0,
        .catbuf = NULL,
    };

    // Initialise the raw lex context.
    init_raw_lex_context(&ctx_out->raw_ctx, input);
}

enum sh_lex_result lex(struct sh_lex_context *ctx) {
    // Allocate memory for the concatenation buffer if not yet done.
    if (ctx->catbuf == NULL) {
        size_t new_catbuf_capacity = 16;
        char *new_catbuf = malloc(sizeof(char) * new_catbuf_capacity);
        if (new_catbuf == NULL) {
          return SH_LEX_MEMORY_ERROR;
        }

        ctx->catbuf_capacity = new_catbuf_capacity;
        ctx->catbuf = new_catbuf;
        ctx->catbuf[0] = '\0';
    }

    // Keep track of the return value.
    enum sh_lex_result result = SH_LEX_ONGOING;

    // Get the next raw token.
    struct sh_raw_token raw_token;
    enum sh_raw_lex_result raw_lex_result = raw_lex(&ctx->raw_ctx, &raw_token);

    // Decide whether to continue based on the results of the raw lex.
    switch (raw_lex_result) {
    case SH_RAW_LEX_END:
        result = SH_LEX_END;
        goto ret;
    case SH_RAW_LEX_MEMORY_ERROR:
        result = SH_LEX_MEMORY_ERROR;
        goto ret;
    case SH_RAW_LEX_GLOB_ERROR:
        result = SH_LEX_GLOB_ERROR;
        goto ret;
    case SH_RAW_LEX_ONGOING:
        break;
    }

    assert(raw_lex_result == SH_RAW_LEX_ONGOING);

    // Determine which state to change to.
    enum sh_lex_state old_state = ctx->state;
    if (ctx->escape) {
        // Don't do any state transitions if we're escaping the current token.
        assert(
            old_state == SH_LEX_STATE_WORD_QUOTED
            || old_state == SH_LEX_STATE_WORD_UNQUOTED
        );
    } else if (old_state == SH_LEX_STATE_WORD_QUOTED
        && raw_token.type == ctx->start_quote.type)
    {
        // Reached the closing quote for a quoted string.
        ctx->state = SH_LEX_STATE_WORD_QUOTED_END;
    } else if (old_state == SH_LEX_STATE_WORD_QUOTED && raw_token.type == SH_TOKEN_END)
    {
        // Reached the end of the token sequence even though we haven't
        // terminated the current quoted string.
        result = SH_LEX_UNTERMINATED_QUOTE;
        goto ret;
    } else if (old_state == SH_LEX_STATE_WORD_QUOTED) {
        // No state transition — the only way to leave the quoted state is to
        // see the end quote.
    } else if (is_unquoted_section_marker(raw_token.type)) {
        // Start of an unquoted section of a word.
        ctx->state = SH_LEX_STATE_WORD_UNQUOTED;
    } else if (raw_token.type == SH_RAW_TOKEN_DOUBLE_QUOTE || raw_token.type == SH_RAW_TOKEN_SINGLE_QUOTE)
    {
        // Start of a quoted section of a word.
        ctx->state = SH_LEX_STATE_WORD_QUOTED;
        ctx->start_quote = raw_token;

        // We don't actually need to do anything with the opening quote.
        result = SH_LEX_ONGOING;
        goto ret;
    } else {
        // Everything else would just be the dull state.
        ctx->state = SH_LEX_STATE_DULL;
    }

    // If we exited a word, then write the token.
    if ((old_state == SH_LEX_STATE_WORD_QUOTED_END
         || old_state == SH_LEX_STATE_WORD_UNQUOTED)
        && (ctx->state == SH_LEX_STATE_DULL || raw_token.type == SH_TOKEN_END))
    {
        enum sh_end_word_result end_word_result = end_word(ctx);
        switch (end_word_result) {
        case SH_END_WORD_MEMORY_ERROR:
            result = SH_LEX_MEMORY_ERROR;
            goto ret;
        case SH_END_WORD_GLOB_ABORTED:
            result = SH_LEX_GLOB_ERROR;
            goto ret;
        case SH_END_WORD_SUCCESS:
            break;
        }
    }

    // Perform the actions for the current state.
    switch (ctx->state) {
    case SH_LEX_STATE_DULL: {
        // Check if the token is a whitespace token. Whitespace tokens are
        // thrown away for the dull state.
        if (raw_token.type == SH_RAW_TOKEN_WHITESPACE) {
            break;
        }

        // At this point, the token must be one of the other special characters.

        struct sh_token token = (struct sh_token) {
            .type = token_type_from_raw_token_type(raw_token.type),
            .text = raw_token.text, // This is guaranteed to be statically
                                    // allocated, so no copying is necessary.
        };

        if (append_to_tokbuf(ctx, token) != SH_APPEND_SUCCESS) {
            result = SH_LEX_MEMORY_ERROR;
            goto ret;
        }

        break;
    }

    case SH_LEX_STATE_WORD_QUOTED:
    case SH_LEX_STATE_WORD_UNQUOTED: {
        // Handle escaped characters.
        if (ctx->escape) {
            // Special case for `2>`. We need to escape the `2` and add it to
            // the word, but create a separate special for token `>`.
            if (ctx->state == SH_LEX_STATE_WORD_UNQUOTED
                && raw_token.type == SH_TOKEN_2_ANGLE_BRACKET_R)
            {
                // Append the escaped "2".
                if (append_to_catbuf(ctx, "2", 1) != SH_APPEND_SUCCESS) {
                    result = SH_LEX_MEMORY_ERROR;
                    goto ret;
                }

                // We need to end the current word since `>` is a special token.
                enum sh_end_word_result end_word_result = end_word(ctx);
                switch (end_word_result) {
                case SH_END_WORD_MEMORY_ERROR:
                    result = SH_LEX_MEMORY_ERROR;
                    goto ret;
                case SH_END_WORD_GLOB_ABORTED:
                    result = SH_LEX_GLOB_ERROR;
                    goto ret;
                case SH_END_WORD_SUCCESS:
                    break;
                }

                // Create and append the `>` token.
                struct sh_token angle_r_tok = (struct sh_token) {
                    .type = SH_TOKEN_ANGLE_BRACKET_R,
                    .text = ">",
                };

                if (append_to_tokbuf(ctx, angle_r_tok) != SH_APPEND_SUCCESS) {
                    result = SH_LEX_MEMORY_ERROR;
                    goto ret;
                }

                // This is the only time we'll change the state from here.
                ctx->state = SH_LEX_STATE_DULL;
            } else if (append_to_catbuf(ctx, raw_token.text, strlen(raw_token.text)) != SH_APPEND_SUCCESS)
            {
                result = SH_LEX_MEMORY_ERROR;
                goto ret;
            }

            ctx->escape = false;
            break;
        }

        // If the token is a backslash, then we need to escape the next token.
        if (raw_token.type == SH_RAW_TOKEN_BACKSLASH) {
            // Backslash followed by any character will make that character be
            // taken literally, so we can just add a backslash unconditionally.
            // Keep in mind that we are building a string to pass to `glob()`.
            if (append_to_catbuf(ctx, "\\", 1) != SH_APPEND_SUCCESS) {
                result = SH_LEX_MEMORY_ERROR;
                goto ret;
            }
            ctx->escape = true;
            break;
        }

        // Otherwise, we need to add the token's text.
        // However, if we're inside a quote, characters special to `glob()` need
        // to be escaped.
        if (ctx->state == SH_LEX_STATE_WORD_QUOTED
            && (raw_token.type == SH_RAW_TOKEN_ASTERISK
                || raw_token.type == SH_RAW_TOKEN_QUESTION
                || raw_token.type == SH_RAW_TOKEN_SQUARE_BRACKET_L)
            && append_to_catbuf(ctx, "\\", 1) != SH_APPEND_SUCCESS)
        {
            result = SH_LEX_MEMORY_ERROR;
            goto ret;
        }

        // Finally, add the text of the current token.
        if (append_to_catbuf(ctx, raw_token.text, strlen(raw_token.text))
            != SH_APPEND_SUCCESS)
        {
            result = SH_LEX_MEMORY_ERROR;
            goto ret;
        }

        break;
    }

    case SH_LEX_STATE_WORD_QUOTED_END:
        break;
    }

ret:
    if (result != SH_LEX_END) {
        destroy_raw_token(&raw_token);
    }
    return result;
}

void destroy_lex_context(struct sh_lex_context *ctx) {
    free(ctx->catbuf);
    ctx->catbuf = NULL;
    ctx->catbuf_len = 0;
    ctx->catbuf_capacity = 0;

    for (size_t idx = 0; idx < ctx->tokbuf_len; idx++) {
        destroy_token(&ctx->tokbuf[idx]);
    }

    free(ctx->tokbuf);
    ctx->tokbuf = NULL;
    ctx->tokbuf_len = 0;
    ctx->tokbuf_capacity = 0;
}

void destroy_token(struct sh_token *token) {
    // Only the `text` `SH_TOKEN_WORD` is dynamically allocated. Other token
    // types use static allocation.
    if (token->type == SH_TOKEN_WORD) {
        // NOTE: This is a safe cast since we are no longer using it. Strangely,
        // `free()` takes in a non-const pointer. Linus Torvalds (creator of
        // Linux) seems to agree that `free()` shouldn't take in a non-const
        // pointer.
        free((char *) token->text);
        token->text = NULL;
    }
}

enum sh_token_type
token_type_from_raw_token_type(enum sh_raw_token_type raw_token_type) {
    switch (raw_token_type) {
    case SH_RAW_TOKEN_AMP:
        return SH_TOKEN_AMP;
    case SH_RAW_TOKEN_SEMICOLON:
        return SH_TOKEN_SEMICOLON;
    case SH_RAW_TOKEN_EXCLAM:
        return SH_TOKEN_EXCLAM;
    case SH_RAW_TOKEN_PIPE:
        return SH_TOKEN_PIPE;
    case SH_RAW_TOKEN_ANGLE_BRACKET_L:
        return SH_TOKEN_ANGLE_BRACKET_L;
    case SH_RAW_TOKEN_ANGLE_BRACKET_R:
        return SH_TOKEN_ANGLE_BRACKET_R;
    case SH_RAW_TOKEN_2_ANGLE_BRACKET_R:
        return SH_TOKEN_2_ANGLE_BRACKET_R;
    case SH_RAW_TOKEN_END:
        return SH_TOKEN_END;
    default:
        assert(false);
    }
}

enum sh_append_result append_to_catbuf(
    struct sh_lex_context *ctx,
    char const *text,
    size_t text_len
) {
    // Grow the buffer if needed.
    // `+ 1` for null character.
    if (ctx->catbuf_len + text_len + 1 > ctx->catbuf_capacity) {
        size_t new_capacity = ctx->catbuf_capacity + text_len * 2 + 1;
        char *tmp = realloc(ctx->catbuf, sizeof(char) * new_capacity);
        if (tmp == NULL) {
            return SH_APPEND_MEMORY_ERROR;
        }

        ctx->catbuf_capacity = new_capacity;
        ctx->catbuf = tmp;
    }

    // Append the text to the buffer.
    strncpy(ctx->catbuf + ctx->catbuf_len, text, text_len);
    ctx->catbuf_len += text_len;
    ctx->catbuf[ctx->catbuf_len] = '\0';
    return SH_APPEND_SUCCESS;
}

enum sh_append_result
append_to_tokbuf(struct sh_lex_context *ctx, struct sh_token token) {
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
            return SH_APPEND_MEMORY_ERROR;
        }

        ctx->tokbuf_capacity = new_capacity;
        ctx->tokbuf = tmp;
    }

    // Append the token to the buffer.
    ctx->tokbuf[ctx->tokbuf_len] = token;
    ctx->tokbuf_len++;

    return SH_APPEND_SUCCESS;
}

enum sh_end_word_result end_word(struct sh_lex_context *ctx) {
    // Keep track of the result.
    enum sh_end_word_result result = SH_END_WORD_SUCCESS;

    // Expand globs.
    glob_t pg;
    int glob_ret = glob(ctx->catbuf, 0, NULL, &pg);

    if (glob_ret == GLOB_NOSPACE) {
        result = SH_END_WORD_MEMORY_ERROR;
        goto ret;
    }

    if (glob_ret == GLOB_ABORTED) {
        result = SH_END_WORD_GLOB_ABORTED;
        goto ret;
    }

    // If there is no match or any other error, we follow bash's behaviour and
    // treat the word literally.
    if (glob_ret == GLOB_NOMATCH || glob_ret < 0) {
        // We must first remove backslashes since they are no longer needed.
        char *pread = ctx->catbuf;
        char *pwrite = ctx->catbuf;
        while (*pread != '\0') {
            if (*pread != '\\') {
                *pwrite = *pread;
                pwrite++;
            }
            pread++;
        }
        *pwrite = '\0';

        // Create and append the token.
        struct sh_token token = (struct sh_token) {
            .type = SH_TOKEN_WORD,
            .text = ctx->catbuf,
        };

        if (append_to_tokbuf(ctx, token) != SH_APPEND_SUCCESS) {
            result = SH_END_WORD_MEMORY_ERROR;
            goto ret;
        }

        // Don't free `catbuf` because `token.text` points to it!
        ctx->catbuf_capacity = 0;
        ctx->catbuf_len = 0;
        ctx->catbuf = NULL;

        result = SH_END_WORD_SUCCESS;
        goto ret;
    }

    // At this point, the `glob()` must have been successful.
    assert(glob_ret == 0);
    for (size_t idx = 0; idx < pg.gl_pathc; idx++) {
        // We don't want to use the pointers from `pg.gl_pathv` directly because
        // `pg` needs to be cleaned up with `globfree()`, so we copy each glob
        // result.
        size_t text_len = strlen(pg.gl_pathv[idx]);
        char *text = malloc(sizeof(char) * (text_len + 1));
        if (text == NULL) {
            result = SH_END_WORD_MEMORY_ERROR;
            goto ret;
        }

        // Copy the characters into `text`.
        strncpy(text, pg.gl_pathv[idx], text_len);
        text[text_len] = '\0';

        // Create and append the token.
        struct sh_token token = (struct sh_token) {
            .type = SH_TOKEN_WORD,
            .text = text,
        };

        if (append_to_tokbuf(ctx, token) != SH_APPEND_SUCCESS) {
            result = SH_END_WORD_MEMORY_ERROR;
            goto ret;
        }
    }

ret:
    // No need to free `catbuf` since it can be reused.
    // Let `destroy_lex_context()` handle the freeing.
    ctx->catbuf_len = 0;

    globfree(&pg);
    return result;
}

bool is_unquoted_section_marker(enum sh_raw_token_type raw_tok_type) {
    return raw_tok_type == SH_RAW_TOKEN_TEXT
           || raw_tok_type == SH_RAW_TOKEN_BACKSLASH
           || raw_tok_type == SH_RAW_TOKEN_ASTERISK
           || raw_tok_type == SH_RAW_TOKEN_QUESTION
           || raw_tok_type == SH_RAW_TOKEN_SQUARE_BRACKET_L;
}
