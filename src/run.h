/**
 * @file run.h
 *
 * Declarations for running commands.
 */

#ifndef RUN_H
#define RUN_H

#include <stdbool.h>

#include "shell.h"

/**
 * Runs a given command line.
 *
 * This function takes a command line string and executes it.
 *
 * @param ctx the shell context
 * @param line the command line string to run
 */
void run(struct sh_shell_context *ctx, char const *line);

#endif
