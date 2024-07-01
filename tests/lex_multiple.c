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
        struct sh_lex_context ctx;
        init_lex_context(&ctx, inputs[idx]);

        while (lex(&ctx) == SH_LEX_ONGOING)
            ;

        ASSERT_EQ(ctx.tokbuf_len, 9);

        ASSERT_EQ(ctx.tokbuf[0].type, SH_TOKEN_AMP);
        ASSERT_EQ(strcmp(ctx.tokbuf[0].text, "&"), 0);

        ASSERT_EQ(ctx.tokbuf[1].type, SH_TOKEN_SEMICOLON);
        ASSERT_EQ(strcmp(ctx.tokbuf[1].text, ";"), 0);

        ASSERT_EQ(ctx.tokbuf[2].type, SH_TOKEN_EXCLAM);
        ASSERT_EQ(strcmp(ctx.tokbuf[2].text, "!"), 0);

        ASSERT_EQ(ctx.tokbuf[3].type, SH_TOKEN_PIPE);
        ASSERT_EQ(strcmp(ctx.tokbuf[3].text, "|"), 0);

        ASSERT_EQ(ctx.tokbuf[4].type, SH_TOKEN_ANGLE_BRACKET_L);
        ASSERT_EQ(strcmp(ctx.tokbuf[4].text, "<"), 0);

        ASSERT_EQ(ctx.tokbuf[5].type, SH_TOKEN_ANGLE_BRACKET_R);
        ASSERT_EQ(strcmp(ctx.tokbuf[5].text, ">"), 0);

        ASSERT_EQ(ctx.tokbuf[6].type, SH_TOKEN_2_ANGLE_BRACKET_R);
        ASSERT_EQ(strcmp(ctx.tokbuf[6].text, "2>"), 0);

        ASSERT_EQ(ctx.tokbuf[7].type, SH_TOKEN_WORD);
        ASSERT_EQ(strcmp(ctx.tokbuf[7].text, "foobar123helloworld"), 0);

        ASSERT_EQ(ctx.tokbuf[8].type, SH_TOKEN_END);
        ASSERT_EQ(strcmp(ctx.tokbuf[8].text, "\0"), 0);

        ASSERT_EQ(lex(&ctx), SH_LEX_END);

        destroy_lex_context(&ctx);
    }
}
