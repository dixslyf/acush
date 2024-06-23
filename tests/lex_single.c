#include <string.h>

#include "assertions.h"
#include "lex.h"

size_t const input_count = 8;
char const *const inputs[] = {"&", ";", "!", "|", "<", ">", "2>", "foobar123"};
enum sh_token_type const types[] = {
    SH_TOKEN_AMP,
    SH_TOKEN_SEMICOLON,
    SH_TOKEN_EXCLAM,
    SH_TOKEN_PIPE,
    SH_TOKEN_ANGLE_BRACKET_L,
    SH_TOKEN_ANGLE_BRACKET_R,
    SH_TOKEN_2_ANGLE_BRACKET_R,
    SH_TOKEN_WORD,
};

int main() {
    for (size_t idx = 0; idx < input_count; idx++) {
        struct sh_lex_context *ctx = init_lex_context(inputs[idx]);
        struct sh_token token;
        ASSERT_EQ(lex(ctx, &token), SH_LEX_ONGOING);
        ASSERT_EQ(token.type, types[idx]);
        ASSERT_EQ(strcmp(token.text, inputs[idx]), 0);
        destroy_token(&token);

        ASSERT_EQ(lex(ctx, &token), SH_LEX_END);

        destroy_lex_context(ctx);
    }
}
