#include <string.h>

#include "assertions.h"
#include "lex.h"

#define INPUT_BUFSIZE 128

int main() {
    char *inputs[] = {
        "&;!|<>2>foobar123\"hello\"'world'\"\"''",
        "& \n\t\f\r\v; \n\t\f\r\v! \n\t\f\r\v| \n\t\f\r\v< \n\t\f\r\v> "
        "\n\t\f\r\v2> \n\t\f\r\vfoobar123\"hello\"'world'\"\"''"
    };

    for (size_t idx = 0; idx < 2; idx++) {
        struct sh_lex_context *ctx = init_lex_context(inputs[idx]);
        struct sh_token token;

        ASSERT_EQ(lex(ctx, &token), SH_LEX_ONGOING);
        ASSERT_EQ(token.type, SH_TOKEN_AMP);
        ASSERT_EQ(strcmp(token.text, "&"), 0);
        destroy_token(&token);

        ASSERT_EQ(lex(ctx, &token), SH_LEX_ONGOING);
        ASSERT_EQ(token.type, SH_TOKEN_SEMICOLON);
        ASSERT_EQ(strcmp(token.text, ";"), 0);
        destroy_token(&token);

        ASSERT_EQ(lex(ctx, &token), SH_LEX_ONGOING);
        ASSERT_EQ(token.type, SH_TOKEN_EXCLAM);
        ASSERT_EQ(strcmp(token.text, "!"), 0);
        destroy_token(&token);

        ASSERT_EQ(lex(ctx, &token), SH_LEX_ONGOING);
        ASSERT_EQ(token.type, SH_TOKEN_PIPE);
        ASSERT_EQ(strcmp(token.text, "|"), 0);
        destroy_token(&token);

        ASSERT_EQ(lex(ctx, &token), SH_LEX_ONGOING);
        ASSERT_EQ(token.type, SH_TOKEN_ANGLE_BRACKET_L);
        ASSERT_EQ(strcmp(token.text, "<"), 0);
        destroy_token(&token);

        ASSERT_EQ(lex(ctx, &token), SH_LEX_ONGOING);
        ASSERT_EQ(token.type, SH_TOKEN_ANGLE_BRACKET_R);
        ASSERT_EQ(strcmp(token.text, ">"), 0);
        destroy_token(&token);

        ASSERT_EQ(lex(ctx, &token), SH_LEX_ONGOING);
        ASSERT_EQ(token.type, SH_TOKEN_2_ANGLE_BRACKET_R);
        ASSERT_EQ(strcmp(token.text, "2>"), 0);
        destroy_token(&token);

        ASSERT_EQ(lex(ctx, &token), SH_LEX_ONGOING);
        ASSERT_EQ(token.type, SH_TOKEN_WORD);
        ASSERT_EQ(strcmp(token.text, "foobar123helloworld"), 0);
        destroy_token(&token);

        ASSERT_EQ(lex(ctx, &token), SH_LEX_END);
        destroy_lex_context(ctx);
    }
}
