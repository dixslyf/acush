#include "assertions.h"
#include "lex.h"

int main() {
    struct sh_lex_context *ctx = init_lex_context(" \n\t\f\r\v");
    struct sh_token token;
    ASSERT_EQ(lex(ctx, &token), SH_LEX_END);
    destroy_lex_context(ctx);
}
