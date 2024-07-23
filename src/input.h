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

/**
 * Enables raw mode for terminal input.
 *
 * This function modifies the terminal attributes to enable raw mode,
 * which disables canonical mode and echo.
 *
 * @param orig_termios a pointer to store the original terminal attributes
 * @return true if successful, false otherwise
 */
bool enable_raw_mode(struct termios *orig_termios);

/**
 * Restores the terminal to its original mode.
 *
 * This function restores the terminal attributes to the state they were in
 * before raw mode was enabled.
 *
 * @param orig_termios a pointer to the original terminal attributes
 * @return true if successful, false otherwise
 */
bool restore_term_mode(struct termios const *orig_termios);

/**
 * Reads user input from the terminal.
 *
 * This function reads user input from the terminal and stores it in the
 * provided buffer. This function allocates memory — callers should free the
 * memory once it is no longer needed.
 *
 * @param ctx the shell context
 * @param out a pointer to the buffer to store the input
 * @param out_capacity a pointer to store the size of the buffer
 * @return the number of bytes read into the buffer, or -1 on error
 */
ssize_t read_input(
    struct sh_shell_context const *ctx,
    char **out,
    size_t *out_capacity
);

#endif
