#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "parse.h"
#include "run.h"
#include "shell.h"

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
    size_t redirection_count;
    struct sh_redirection_desc *redirections;

    size_t argc;
    char const *const *argv;

    struct sh_pipe_desc pipe_desc;
};

void run_ast(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_ast_root const *root
);

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

pid_t run_cmd(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_ast_cmd const *cmd,
    pid_t pgid,
    enum sh_job_type job_type,
    struct sh_pipe_desc pipe_desc
);

int run_builtin_fg(struct sh_shell_context *ctx, struct sh_spawn_desc desc);

pid_t spawn(
    struct sh_shell_context *ctx,
    pid_t pgid,
    struct sh_spawn_desc desc
);

struct sh_run_result run(struct sh_shell_context *ctx, char *line) {
    // TODO: don't print error messages here — propagate errors to the caller.

    struct sh_run_result result = (struct sh_run_result) {
        .error_count = 0,
        .errors = NULL,
    };

    struct sh_lex_context lex_ctx;
    init_lex_context(&lex_ctx, line);

    enum sh_lex_result lex_result;
    do {
        lex_result = lex(&lex_ctx);
    } while (lex_result == SH_LEX_ONGOING);

    if (lex_result == SH_LEX_MEMORY_ERROR) {
        fprintf(stderr, "error: memory failure\n");
    } else if (lex_result == SH_LEX_UNTERMINATED_QUOTE) {
        fprintf(stderr, "error: unterminated quote\n");
    } else if (lex_result == SH_LEX_UNTERMINATED_QUOTE) {
        fprintf(stderr, "error: glob error\n");
    } else {
        struct sh_ast_root ast;
        enum sh_parse_result parse_result = parse(
            lex_ctx.tokbuf,
            lex_ctx.tokbuf_len,
            &ast
        );

        if (parse_result != SH_PARSE_SUCCESS) {
            printf("error: failed to parse command line\n");
        } else {
            run_ast(ctx, &result, &ast);
            destroy_ast(&ast);
        }
    }
    destroy_lex_context(&lex_ctx);

    return result;
}

void run_ast(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_ast_root const *root
) {
    run_cmd_line(ctx, result, &root->cmd_line);
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
    // Since the SIGCHLD handler consumes child processes, we need to block
    // SIGCHLD first so that we can properly wait for the child processes
    // here.
    sigset_t sigchld_set;
    sigemptyset(&sigchld_set);
    int sigaddset_ret = sigaddset(&sigchld_set, SIGCHLD);
    assert(sigaddset_ret == 0);
    int sigprocmask_ret = sigprocmask(SIG_BLOCK, &sigchld_set, NULL);
    // Seems like the only way for `sigprocmask` (and `sigemptyset` and
    // `sigaddset`) to fail is programmer error.
    assert(sigprocmask_ret == 0);

    struct sh_ast_job const *job = &job_desc->job;
    pid_t pids[job->cmd_count];
    pid_t pids_len = 0;
    pid_t pgid = 0;

    // If there is only one command, no piping is required.
    if (job->cmd_count == 1) {
        struct sh_pipe_desc pipe_desc = (struct sh_pipe_desc) {
            .redirect_stdin = false,
            .redirect_stdout = false,
        };

        pid_t pid = run_cmd(
            ctx,
            result,
            &job->piped_cmds[0],
            pgid,
            job_desc->type,
            pipe_desc
        );

        // The command was spawned successfully and is not a foreground builtin.
        if (pid > 0) {
            // Keep track of the PID.
            pids[0] = pid;
            pids_len++;

            // Set the group ID to the PID.
            pgid = pid;
        }
    } else {
        // Multiple commands, so piping is required.
        int pipes[job->cmd_count - 1][2];
        size_t pipes_len = 0;
        for (size_t idx = 0; idx < job->cmd_count; idx++) {
            struct sh_pipe_desc pipe_desc;

            // Redirect stdin, but not for the first job.
            pipe_desc.redirect_stdin = idx != 0;
            if (pipe_desc.redirect_stdin) {
                pipe_desc.read_fd_left = pipes[idx - 1][0];
                pipe_desc.write_fd_left = pipes[idx - 1][1];
            }

            // Redirect stdout, but not for the last job.
            pipe_desc.redirect_stdout = idx < job->cmd_count - 1;
            if (pipe_desc.redirect_stdout) {
                // Create a new pipe, but not on the last iteration since there
                // should only be `job->cmd_count - 1` pipes.
                if (pipe(pipes[idx]) < 0) {
                    // Probably not a good idea to continue on failure.
                    break;
                }

                pipes_len++;

                pipe_desc.read_fd_right = pipes[idx][0];
                pipe_desc.write_fd_right = pipes[idx][1];
            }

            pid_t pid = run_cmd(
                ctx,
                result,
                &job->piped_cmds[idx],
                pgid,
                job_desc->type,
                pipe_desc
            );

            // If we failed to spawn the command, then it is probably not a good
            // idea to continue.
            if (pid < 0) {
                break;
            }

            // Keep track of the PID, but only if the command was not a
            // foreground builtin.
            if (pid > 0) {
                pids[pids_len] = pid;
                pids_len++;

                // Set the group ID to the PID of the first spawned command.
                if (pgid == 0) {
                    pgid = pid;
                }
            }

            // If `exit` was called, then we should stop any further processing.
            if (ctx->should_exit) {
                break;
            }
        }

        // If this is true, then that means one of the commands failed to spawn.
        // This means the last pipe will not have been closed properly, so we
        // need to close it.
        if (pipes_len > 0 && pipes_len < job->cmd_count - 1) {
            if (close(pipes[pipes_len - 1][0]) < 0) {
                // TODO: handle error
            }

            if (close(pipes[pipes_len - 1][1]) < 0) {
                // TODO: handle error
            }
        }
    }

    // When the job is a foreground job and processes were spawned, we want to
    // set the job as the terminal foreground process group. We also want to
    // wait for all processes in the job to finish.
    //
    // Background jobs are consumed by the signal handler for
    // `SIGCHLD` so that they don't become zombie processes.
    if (job_desc->type == SH_JOB_FG && pids_len > 0) {
        // Set the terminal foreground process group to the job's process group.
        if (tcsetpgrp(STDIN_FILENO, pgid) < 0) {
            perror("tcsetpgrp");
        }

        // Wait for the processes.
        size_t wait_successes = 0;
        // TODO: what to do with `info`?
        siginfo_t info;
        while (wait_successes < pids_len) {
            pid_t wait_ret = waitid(P_PGID, pgid, &info, WEXITED | WSTOPPED);
            if (wait_ret < 0) {
                assert(wait_ret < 0);
                perror("waitid");

                // To prevent getting into an infinite loop, we should break out
                // of the loop. This does leave the possibility of zombie
                // processes, but the SIGCHLD handler should clean them up.
                break;
            } else {
                wait_successes++;
            }
        }

        // Set the terminal foreground process group back to the shell process.
        // However, we need to temporarily ignore `SIGTTOU` first because it
        // will be sent when `tcsetpgrp()` is called from a background process,
        // and our shell process is now a background process.
        struct sigaction sigact_ign;
        struct sigaction sigact_ttou_old;
        sigemptyset(&sigact_ign.sa_mask);
        sigact_ign.sa_flags = 0;
        sigact_ign.sa_handler = SIG_IGN;
        sigaction(SIGTTOU, &sigact_ign, &sigact_ttou_old);

        if (tcsetpgrp(STDIN_FILENO, getpgid(0)) < 0) {
            perror("tcsetpgrp");
        }

        // Restore the handler for SIGTTOU.
        sigaction(SIGTTOU, &sigact_ttou_old, NULL);
    }

    // Unblock SIGCHLD since we're done with waiting.
    sigprocmask_ret = sigprocmask(SIG_UNBLOCK, &sigchld_set, NULL);
    assert(sigprocmask_ret == 0);
}

pid_t run_cmd(
    struct sh_shell_context *ctx,
    struct sh_run_result *result,
    struct sh_ast_cmd const *cmd,
    pid_t pgid,
    enum sh_job_type job_type,
    struct sh_pipe_desc pipe_desc
) {
    size_t argc = cmd->simple_cmd.argc;
    char const *const *argv = cmd->simple_cmd.argv;
    assert(argc != 0);

    // Create a description for spawning the command.
    struct sh_spawn_desc desc = {
        .redirection_count = cmd->redirection_count,
        .redirections = cmd->redirections,
        .argc = argc,
        .argv = argv,
        .pipe_desc = pipe_desc,
    };

    // Handle running builtins in the foreground.
    if (job_type == SH_JOB_FG && is_builtin(argv[0])) {
        // TODO: error handling
        run_builtin_fg(ctx, desc);
        return 0;
    }

    // Run non-builtins. Also run background built-ins.
    // TODO: write error to `result` on error
    pid_t pid = spawn(ctx, pgid, desc);
    return pid;
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

        int *fds_mem;
        int std_fileno;
        switch (redir.type) {
        case SH_REDIRECT_STDOUT:
            fds_mem = &fds.stdout;
            std_fileno = STDOUT_FILENO;
            break;
        case SH_REDIRECT_STDIN:
            fds_mem = &fds.stdin;
            std_fileno = STDIN_FILENO;
            break;
        case SH_REDIRECT_STDERR:
            fds_mem = &fds.stderr;
            std_fileno = STDERR_FILENO;
            break;
        }

        // If we're overwriting a previous redirection, close it.
        if (*fds_mem != std_fileno && close(*fds_mem) < 0) {
            // TODO: handle error
        }
        *fds_mem = fd_to;
    }

    run_builtin(ctx, fds, desc.argc, desc.argv);

    // Close file descriptors if there were redirections.
    if (fds.stdout != STDOUT_FILENO && close(fds.stdout) < 0) {
        // TODO: handle error
    }

    if (fds.stdin != STDIN_FILENO && close(fds.stdin) < 0) {
        // TODO: handle error
    }

    if (fds.stderr != STDERR_FILENO && close(fds.stderr) < 0) {
        // TODO: handle error
    }

    return 0;
}

pid_t spawn(
    struct sh_shell_context *ctx,
    pid_t pgid,
    struct sh_spawn_desc desc
) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process.

        // Since we set the signal handler and signal mask in the parent
        // process, the child process will inherit them. We need to "undo" those
        // changes.
        //
        // Reset the signal handler for SIGCHLD to the default.
        struct sigaction sigact_chld;
        sigemptyset(&sigact_chld.sa_mask);
        sigact_chld.sa_flags = 0;
        sigact_chld.sa_handler = SIG_DFL;
        sigaction(SIGCHLD, &sigact_chld, NULL);

        // Unblock SIGCHLD.
        sigset_t sigchld_set;
        sigemptyset(&sigchld_set);
        int sigaddset_ret = sigaddset(&sigchld_set, SIGCHLD);
        assert(sigaddset_ret == 0);
        int sigprocmask_ret = sigprocmask(SIG_UNBLOCK, &sigchld_set, NULL);
        assert(sigprocmask_ret == 0);

        // Similarly, we want to reset the signal handlers for SIGINT, SIGQUIT
        // and SIGTSTP.
        reset_signal_handlers_for_stop_signals();

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

            // Once the redirection is done, we don't need the original file
            // descriptor around, so we close it. Even if the redirection
            // failed, we still don't need the original file descriptor anymore.
            if (close(desc.pipe_desc.read_fd_left) < 0) {
                // Once the redirection is done, we don't need the original file
                // descriptor around, so we close it.

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

            // Once the redirection is done, we don't need the original file
            // descriptor around, so we close it. Even if the redirection
            // failed, we still don't need the original file descriptor anymore.
            if (close(desc.pipe_desc.write_fd_right) < 0) {
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
                continue;
            }

            int fd_from = redir.type == SH_REDIRECT_STDOUT
                              ? STDOUT_FILENO
                              : (redir.type == SH_REDIRECT_STDIN
                                     ? STDIN_FILENO
                                     : STDERR_FILENO);

            if (dup2(fd_to, fd_from) < 0) {
                // TODO: handle error
            }

            // Once the redirection is done, we don't need the original file
            // descriptor around, so we close it. Even if the redirection
            // failed, we still don't need the original file descriptor anymore.
            if (close(fd_to) < 0) {
                // TODO: handle error
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
        // The cast is safe:
        // http://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html
        execvp(desc.argv[0], (char *const *) desc.argv);

        // This point is only reached if `execvp` failed.
        // There is no point keeping the child process around, so we just print
        // an error message and exit from the child process.
        fprintf(stderr, "%s: command not found\n", desc.argv[0]);
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Parent process.

        // Set the group ID for the child process.
        if (setpgid(pid, pgid) < 0) {
            // TODO: properly handle error
            perror("setpgid");
        }

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
    }

    return pid;
}
