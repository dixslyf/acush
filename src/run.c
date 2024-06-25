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

    enum sh_redirect_type redirect_type;
    char *redirect_file;

    size_t argc;
    char **argv;

    struct sh_pipe_desc pipe_desc;
};

void run_cmd_line(
    struct sh_run_result *result,
    struct sh_ast_cmd_line const *cmd_line
);

void run_job_desc(
    struct sh_run_result *result,
    struct sh_job_desc const *job_desc
);

void run_cmd(
    struct sh_run_result *result,
    struct sh_ast_cmd const *cmd,
    enum sh_job_type job_type,
    struct sh_pipe_desc pipe_desc
);

pid_t spawn(struct sh_spawn_desc desc);

struct sh_run_result run(struct sh_ast_root const *root) {
    struct sh_run_result result = (struct sh_run_result) {
        .should_exit = false,
        .error_count = 0,
        .errors = NULL,
    };

    run_cmd_line(&result, &root->cmd_line);

    return result;
}

void run_cmd_line(
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
        run_job_desc(result, &cmd_line->job_descs[idx]);
    }
}

void run_job_desc(
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
        run_cmd(result, &job->piped_cmds[0], job_desc->type, pipe_desc);
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
        run_cmd(result, &job->piped_cmds[idx], job_desc->type, pipe_desc);
    }
}

void run_cmd(
    struct sh_run_result *result,
    struct sh_ast_cmd const *cmd,
    enum sh_job_type job_type,
    struct sh_pipe_desc pipe_desc
) {
    size_t argc = cmd->simple_cmd.argc;
    char **argv = cmd->simple_cmd.argv;

    // Handle `exit` builtin.
    if (argc >= 1 && strcmp(argv[0], "exit") == 0) {
        struct sh_exit_result exit_result = run_exit(argc, argv);

        if (exit_result.type != SH_EXIT_SUCCESS) {
            // TODO: write error to `result` on error
            return;
        }

        // Don't overwrite the exit code for a previous exit call since that
        // call should have priority.
        if (result->should_exit) {
            return;
        }

        assert(exit_result.type == SH_EXIT_SUCCESS);
        result->should_exit = true;
        result->exit_code = exit_result.exit_code;
        return;
    }

    // Handle non-builtins.
    struct sh_spawn_desc desc = {
        .job_type = job_type,
        .redirect_type = cmd->redirect_type,
        .redirect_file = cmd->redirect_file,
        .argc = argc,
        .argv = argv,
        .pipe_desc = pipe_desc,
    };

    // TODO: write error to `result` on error
    pid_t pid = spawn(desc);
}

pid_t spawn(struct sh_spawn_desc desc) {
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
        if (desc.redirect_type != SH_REDIRECT_NONE) {
            assert(desc.redirect_file != NULL);

            // Open the file to redirect to.
            // If we are redirecting stdin, then we open it read-only.
            // Otherwise, we create and open it write-only.
            int fd_to = open(
                desc.redirect_file,
                desc.redirect_type == SH_REDIRECT_STDIN ? O_RDONLY
                                                        : O_CREAT | O_WRONLY,
                0644
            );

            if (fd_to < 0) {
                // TODO: handle error
            }

            int fd_from = desc.redirect_type == SH_REDIRECT_STDOUT
                              ? STDOUT_FILENO
                              : (desc.redirect_type == SH_REDIRECT_STDIN
                                     ? STDIN_FILENO
                                     : STDERR_FILENO);

            if (dup2(fd_to, fd_from) < 0) {
                // TODO: handle error
                close(fd_to);
            }
        }

        execvp(desc.argv[0], desc.argv);

        // This point is only reached if `execl` failed.
        // There is no point keeping the child process around, so we just exit
        // from the child process.
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
