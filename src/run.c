#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "parse.h"
#include "run.h"

struct sh_pipe_desc {
    bool redirect_stdin;
    union {
        struct {
            int read_fd_left;
            int write_fd_left;
        };
    };

    bool redirect_stdout;
    union {
        struct {
            int read_fd_right;
            int write_fd_right;
        };
    };
};

struct sh_spawn_desc {
    enum sh_job_type job_type;

    size_t redirection_count;
    struct sh_redirection_desc *redirections;

    size_t argc;
    char **argv;

    struct sh_pipe_desc pipe_desc;
};

void run_cmd_line(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_ast_cmd_line const *cmd_line
);

void run_job_desc(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_job_desc const *job_desc
);

void run_cmd(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_ast_cmd const *cmd,
    enum sh_job_type job_type,
    struct sh_pipe_desc pipe_desc
);

int run_builtin_fg(struct sh_shell_context *ctx, struct sh_spawn_desc desc);

pid_t spawn(struct sh_shell_context *ctx, struct sh_spawn_desc desc);

struct sh_run_result
run(struct sh_shell_context *ctx, struct sh_ast_root const *root) {
    struct sh_run_result result = (struct sh_run_result) {
        .error_count = 0,
        .errors = NULL,
    };

    run_cmd_line(ctx, &result, &root->cmd_line);

    return result;
}

void run_cmd_line(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_ast_cmd_line const *cmd_line
) {
    // Empty line — nothing to run.
    if (cmd_line == NULL) {
        return;
    }

    if (cmd_line->type == SH_COMMAND_REPEAT) {
        // TODO: repeat command from history
        return;
    }

    for (size_t idx = 0; idx < cmd_line->job_count; idx++) {
        run_job_desc(ctx, result, &cmd_line->job_descs[idx]);
    }
}

void run_job_desc(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_job_desc const *job_desc
) {
    struct sh_ast_job const *job = &job_desc->job;

    // If there is only one command, no piping is required.
    if (job->cmd_count == 1) {
        struct sh_pipe_desc pipe_desc = (struct sh_pipe_desc) {
            .redirect_stdin = false,
            .redirect_stdout = false,
        };
        run_cmd(ctx, result, &job->piped_cmds[0], job_desc->type, pipe_desc);
        return;
    }

    // Multiple commands, so piping is required.
    int pipe_fds[2];
    for (size_t idx = 0; idx < job->cmd_count; idx++) {
        struct sh_pipe_desc pipe_desc;

        // Redirect stdin, but not for the first job.
        pipe_desc.redirect_stdin = idx != 0;
        if (pipe_desc.redirect_stdin) {
            pipe_desc.read_fd_left = pipe_fds[0];
            pipe_desc.write_fd_left = pipe_fds[1];
        }

        // Redirect stdout, but not for the last job.
        pipe_desc.redirect_stdout = idx < job->cmd_count - 1;
        if (pipe_desc.redirect_stdout) {
            // Create a new pipe, but not on the last iteration since there
            // should only be `job->cmd_count - 1` pipes.
            if (pipe(pipe_fds) < 0) {
                // TODO: error handling
            }

            if (pipe_desc.redirect_stdout) {
                pipe_desc.read_fd_right = pipe_fds[0];
                pipe_desc.write_fd_right = pipe_fds[1];
            }
        }

        // TODO: handle failure — need to close pipes
        run_cmd(ctx, result, &job->piped_cmds[idx], job_desc->type, pipe_desc);

        // If `exit` was called, then we should stop any further processing.
        if (ctx->should_exit) {
            break;
        }
    }
}

void run_cmd(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_ast_cmd const *cmd,
    enum sh_job_type job_type,
    struct sh_pipe_desc pipe_desc
) {
    size_t argc = cmd->simple_cmd.argc;
    char **argv = cmd->simple_cmd.argv;
    assert(argc != 0);

    // Create a description for spawning the command.
    struct sh_spawn_desc desc = {
        .job_type = job_type,
        .redirection_count = cmd->redirection_count,
        .redirections = cmd->redirections,
        .argc = argc,
        .argv = argv,
        .pipe_desc = pipe_desc,
    };

    // Handle running builtins in the foreground.
    if (desc.job_type == SH_JOB_FG && is_builtin(argv[0])) {
        // TODO: error handling
        run_builtin_fg(ctx, desc);
        return;
    }

    // Run non-builtins. Also run background builtins.
    // TODO: write error to `result` on error
    pid_t pid = spawn(ctx, desc);
}

int run_builtin_fg(struct sh_shell_context *ctx, struct sh_spawn_desc desc) {
    // Keep track of the file descriptors of the standard streams for the
    // builtins.
    struct sh_builtin_std_fds fds = (struct sh_builtin_std_fds) {
        .stdin = STDIN_FILENO,
        .stdout = STDOUT_FILENO,
        .stderr = STDERR_FILENO,
    };

    // Handle redirection of stdin for piping.
    if (desc.pipe_desc.redirect_stdin) {
        // Close the write end.
        // We can close this safely because the previous command would already
        // have inherited the write end of the pipe if it was spawned in a child
        // process.
        if (close(desc.pipe_desc.write_fd_left) < 0) {
            // TODO: handle error
        }

        // Redirect stdin to the read end.
        fds.stdin = desc.pipe_desc.read_fd_left;
    }

    // Handle redirection of stdout for piping.
    // NOTE: We need to be careful here. We must *not* close the read end of
    // the pipe yet because the next command might need to spawn in a child
    // process and inherit it. If we closed it now, the next process would not
    // inherit the file. Instead, the closing of the read end of the pipe is
    // handled at the end of `spawn()`.
    if (desc.pipe_desc.redirect_stdout) {
        // Redirect stdout to the write end.
        fds.stdout = desc.pipe_desc.write_fd_right;
    }

    // Handle redirection for `>`, `<` and `2>`.
    // Note that, in bash, redirection for `>`, `<` and `2>` has higher
    // priority than redirection for piping, so the redirection here will
    // "overwrite" the redirection for piping.
    for (size_t idx = 0; idx < desc.redirection_count; idx++) {
        struct sh_redirection_desc redir = desc.redirections[idx];
        assert(redir.file != NULL);

        // Open the file to redirect to.
        // If we are redirecting stdin, then we open it read-only.
        // Otherwise, we create and open it write-only.
        int fd_to = open(
            redir.file,
            redir.type == SH_REDIRECT_STDIN ? O_RDONLY
                                            : O_CREAT | O_WRONLY | O_TRUNC,
            0644
        );

        if (fd_to < 0) {
            // TODO: handle error
        }

        switch (redir.type) {
        case SH_REDIRECT_STDOUT:
            fds.stdout = fd_to;
            break;
        case SH_REDIRECT_STDIN:
            fds.stdin = fd_to;
            break;
        case SH_REDIRECT_STDERR:
            fds.stderr = fd_to;
            break;
        }
    }

    run_builtin(ctx, fds, desc.argc, desc.argv);

    // Close pipes.
    // Like `spawn()`, we only close the pipe after executing the consumer
    // side, not after executing the consumer. Hence, we only close the pipe for
    // `desc.pipe_desc.redirect_stdin`.
    if (desc.pipe_desc.redirect_stdin) {
        if (close(desc.pipe_desc.read_fd_left) < 0) {
            // TODO: handle error
        }
        // No need to close the write end since we already closed it earlier.
    }

    // FIXME: close open files

    return 0;
}

pid_t spawn(struct sh_shell_context *ctx, struct sh_spawn_desc desc) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process.

        // Handle redirection of stdin for piping.
        if (desc.pipe_desc.redirect_stdin) {
            // Close the write end.
            if (close(desc.pipe_desc.write_fd_left) < 0) {
                // TODO: handle error
            }

            // Redirect stdin to the read end.
            if (dup2(desc.pipe_desc.read_fd_left, STDIN_FILENO) < 0) {
                // TODO: handle error
            }
        }

        // Handle redirection of stdout for piping.
        if (desc.pipe_desc.redirect_stdout) {
            // Close the read end.
            if (close(desc.pipe_desc.read_fd_right) < 0) {
                // TODO: handle error
            }

            // Redirect stdout to the write end.
            if (dup2(desc.pipe_desc.write_fd_right, STDOUT_FILENO) < 0) {
                // TODO: handle error
            }
        }

        // Handle redirection for `>`, `<` and `2>`.
        // Note that, in bash, redirection for `>`, `<` and `2>` has higher
        // priority than redirection for piping, so the redirection here will
        // "overwrite" the redirection for piping.
        for (size_t idx = 0; idx < desc.redirection_count; idx++) {
            struct sh_redirection_desc redir = desc.redirections[idx];
            assert(redir.file != NULL);

            // Open the file to redirect to.
            // If we are redirecting stdin, then we open it read-only.
            // Otherwise, we create and open it write-only.
            int fd_to = open(
                redir.file,
                redir.type == SH_REDIRECT_STDIN ? O_RDONLY
                                                : O_CREAT | O_WRONLY | O_TRUNC,
                0644
            );

            if (fd_to < 0) {
                // TODO: handle error
            }

            int fd_from = redir.type == SH_REDIRECT_STDOUT
                              ? STDOUT_FILENO
                              : (redir.type == SH_REDIRECT_STDIN
                                     ? STDIN_FILENO
                                     : STDERR_FILENO);

            if (dup2(fd_to, fd_from) < 0) {
                // TODO: handle error
                close(fd_to);
            }
        }

        // Handle builtins that are run in the background.
        if (is_builtin(desc.argv[0])) {
            // No need to change any of these file descriptors since any
            // redirections should already have been handled.
            struct sh_builtin_std_fds fds = (struct sh_builtin_std_fds) {
                .stdin = STDIN_FILENO,
                .stdout = STDOUT_FILENO,
                .stderr = STDERR_FILENO,
            };
            int exit_code = run_builtin(ctx, fds, desc.argc, desc.argv);
            exit(exit_code);
        }

        // Handle non-builtins.
        execvp(desc.argv[0], desc.argv);

        // This point is only reached if `execvp` failed.
        // There is no point keeping the child process around, so we just print
        // an error message and exit from the child process.
        fprintf(stderr, "%s: command not found\n", desc.argv[0]);
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process.

        // The parent process created the pipes for the child processes, but
        // does not actually use them, so we need to close the pipe file
        // descriptors. However, this is a little tricky — we only close the
        // file descriptors for both ends of the pipe after spawning the
        // consumer process (i.e., the "even" process). If we close both ends
        // after spawning the producer process, then the consumer process would
        // not inherit the ends of the pipe.
        //
        // Alternatively, we could close the write end after spawning the
        // producer, then close the read end after spawning the consumer. The
        // producer would inherit both ends of the pipe, but the consumer would
        // only inherit the read end. This is an equally valid approach, but I
        // don't really like the lack of symmetry in this second approach.
        if (desc.pipe_desc.redirect_stdin) {
            if (close(desc.pipe_desc.read_fd_left) < 0) {
                // TODO: handle error
            }

            if (close(desc.pipe_desc.write_fd_left) < 0) {
                // TODO: handle error
            }
        }

        // FIXME: close open files

        // Only wait if the job is a foreground job.
        // Background jobs are consumed by the signal handler for `SIGCHLD`
        // so that they don't become zombie processes.
        if (desc.job_type == SH_JOB_FG) {
            int stat_loc;
            waitpid(pid, &stat_loc, 0);
            // TODO: what to do with `stat_loc`?
        }
    }

    return pid;
}
