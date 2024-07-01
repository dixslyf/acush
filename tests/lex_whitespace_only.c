#include "assertions.h"
#include "lex.h"

#include <string.h>

int main() {
    struct sh_lex_context ctx;
    init_lex_context(&ctx, " \n\t\f\r\v");

    while (lex(&ctx) == SH_LEX_ONGOING)
        ;

    ASSERT_EQ(ctx.tokbuf_len, 1);
    ASSERT_EQ(ctx.tokbuf[0].type, SH_TOKEN_END);
    ASSERT_EQ(strcmp(ctx.tokbuf[0].text, "\0"), 0);
    ASSERT_EQ(lex(&ctx), SH_LEX_END);

    destroy_lex_context(&ctx);
}
