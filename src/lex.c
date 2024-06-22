#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "lex.h"

#define WHITESPACE_DELIMITERS " \n\t\f\r\v"

enum sh_state { SH_STATE_DULL, SH_STATE_QUOTED, SH_STATE_UNQUOTED };

/**
 * Attempts to lex the given character point into a simple special token.
 * `0` is returned and a token corresponding to `*cp` is written to `token_out`
 * if `cp` indeed represents a simple special token. Otherwise, `-1` is
 * returned.
 *
 * See `is_simple_special()` for what counts as a simple special token.
 */
int lex_simple_special(char const *cp, struct sh_token *token_out);

/**
 * Returns `true` if `*cp` is a whitespace delimiter. Otherwise, returns
 * `false`.
 *
 * Whitespace delimiters are those in `WHITESPACE_DELIMITERS`.
 */
bool is_ws_delimiter(char const *cp);

/** Returns `true` if `*cp` is a quote (' or "). Otherwise, returns `false`. */
bool is_quote(char const *cp);

/**
 * Returns `true` if `cp` represents a simple special token.
 *
 * A simple special token is one of the following: & ; | < > 2> ! ' ".
 */
bool is_simple_special(char const *cp);

/**
 * Returns `true` if `*cp` is a word boundary. Otherwise, returns `false`.
 *
 * `*cp` is a word boundary if any of the following evaluate to `true`:
 *   - `is_ws_delimiter(cp)`
 *   - `is_simple_special(cp)`
 *   - `*(cp) == '\0'`
 */
bool is_word_boundary(char const *cp);

ssize_t lex(char const *input, struct sh_token tokens_out[],
            size_t max_tokens) {
  size_t token_idx = 0;

  // The lexing process is implemented as a finite state machine (FSM).
  // We need to initialise the state of the FSM based on the first character of
  // the input.
  enum sh_state state;
  if (is_word_boundary(input)) {
    state = SH_STATE_DULL;
  } else if (is_quote(input)) {
    state = SH_STATE_QUOTED;
  } else {
    state = SH_STATE_UNQUOTED;
  }

  // Set up a buffer for string concatenation for when we are in a word.
  // The worst case scenario is that the entire input is a single word. Hence,
  // the buffer must have a size at least equal to the length of the input
  // string (including the null byte).
  size_t catbuf_capacity = strlen(input) + 1;
  char catbuf[catbuf_capacity];
  size_t catbuf_idx = 0;

  // Keeps track of the start quote when inside a quoted section of a word.
  char const *quote_start = NULL;

  struct sh_token token;
  for (char const *cp = input; *cp != '\0'; cp++) {
    switch (state) {
    case SH_STATE_DULL:
      // Try lexing simple special tokens: & ; ! | < > 2>
      if (lex_simple_special(cp, &token) == 0) {
        tokens_out[token_idx] = token;
        token_idx++;
        if (token.type == SH_TOKEN_2_ANGLE_BRACKET_R) {
          cp++;
        }
      }

      // Peek at the next character to see if we need to change state.

      // Start of quoted section of a word.
      if (is_quote(cp + 1)) {
        state = SH_STATE_QUOTED;
        continue;
      }

      // Start of unquoted section of a word.
      if (!is_word_boundary(cp + 1)) {
        state = SH_STATE_UNQUOTED;
        continue;
      }

      // At this point, the character must be one of the whitespace delimiters.
      // Since we skip them, do nothing.

      break;
    case SH_STATE_QUOTED:
      // The first time we enter this state, `quote_start` should be `NULL` and
      // the character should be a quote.
      if (quote_start == NULL) {
        assert(is_quote(cp));
        quote_start = cp;
        // We don't want to have the quote in the buffer, so we don't add it.
        continue;
      }

      // Handle quote escape sequence.
      if (*cp == '\\' && *(cp + 1) == *quote_start) {
        // Skip the backslash.
        cp++;

        // Add the escaped quote to the buffer.
        catbuf[catbuf_idx] = *cp;
        catbuf_idx++;
        continue;
      }

      // Still in the quoted word, so add the character to the buffer.
      if (*cp != *quote_start) {
        catbuf[catbuf_idx] = *cp;
        catbuf_idx++;
        continue;
      }

      // At this point, `*c` is the closing quote. However, to decide which
      // state to switch to, we need to check if we're still in a word by
      // peeking at the next character.

      // Reset `quote_start` for the next quoted section.
      quote_start = NULL;

      // The next character is another quote, indicating the start of another
      // quoted word.
      if (is_quote(cp + 1)) {
        // No need to change state since we're in a quoted word again.
        continue;
      }

      // If the current character is the last in the current word, then we
      // are done with the current word and can now create the token for it.
      if (is_word_boundary(cp + 1)) {
        // However, if we have an empty pair of quotes ("", ''), then don't
        // create the token.
        if (catbuf_idx != 0) {
          token.type = SH_TOKEN_WORD;

          // We need to dynamically allocate memory this time.
          token.text = malloc(catbuf_idx + 1);
          strncpy(token.text, catbuf, catbuf_idx);
          token.text[catbuf_idx] = '\0';

          tokens_out[token_idx] = token;
          token_idx++;

          catbuf_idx = 0; // Reset for future words.
        }

        state = SH_STATE_DULL;
        continue;
      }

      // If none of the above checks are true, then we must still be within a
      // word, but now we are at the start of an unquoted section of the word.
      // E.g., "foo"bar (the next character would be 'b').
      state = SH_STATE_UNQUOTED;
      break;
    case SH_STATE_UNQUOTED:
      // Handle escape sequences.
      if (*cp == '\\') {
        // Skip the backslash. Now, `cp` points to the escaped character.
        cp++;
      }

      // Add the character to the buffer.
      catbuf[catbuf_idx] = *cp;
      catbuf_idx++;

      // Now, to see if we need to change state, we peek at the next character.

      // If the next character is a quote, then we will still be in the same
      // word, but at the start of a quoted string.
      if (is_quote(cp + 1)) {
        state = SH_STATE_QUOTED;
        continue;
      }

      // If the current character is the last in the word, then we create the
      // token.
      if (is_word_boundary(cp + 1)) {
        token.type = SH_TOKEN_WORD;

        // We need to dynamically allocate memory this time.
        token.text = malloc(catbuf_idx + 1);
        strncpy(token.text, catbuf, catbuf_idx);
        token.text[catbuf_idx] = '\0';

        tokens_out[token_idx] = token;
        token_idx++;

        state = SH_STATE_DULL;
        catbuf_idx = 0; // Reset for future words.
        continue;
      }
    }
  }

  if (state == SH_STATE_QUOTED) {
    return -1;
  }

  return token_idx;
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
  return is_ws_delimiter(cp) || is_simple_special(cp) || *(cp) == '\0';
}
