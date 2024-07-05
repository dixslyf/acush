/**
 * @file input.h
 *
 * Declarations for user input.
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdlib.h>
#include <termios.h>

#include "shell.h"

bool enable_raw_mode(struct termios *orig_termios);

bool restore_term_mode(struct termios const *orig_termios);

ssize_t read_input(
    struct sh_shell_context const *ctx,
    char **out,
    size_t *out_capacity
);

#endif
