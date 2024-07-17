#ifndef RUN_H
#define RUN_H

#include <stdbool.h>
#include <stddef.h>

#include "shell.h"

struct sh_ast_root;
struct sh_shell_context;

void run(struct sh_shell_context *ctx, char const *line);

#endif
