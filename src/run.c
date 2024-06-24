#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "parse.h"
#include "run.h"

struct sh_spawn_desc {
    enum sh_job_type job_type;

    enum sh_redirect_type redirect_type;
    char *redirect_file;

    size_t argc;
    char **argv;
};

void run_cmd_line(
    sh_run_result *result,
    struct sh_ast_cmd_line const *cmd_line
);

void run_job_desc(sh_run_result *result, struct sh_job_desc const *job_desc);

void run_cmd(
    sh_run_result *result,
    struct sh_ast_cmd const *cmd,
    enum sh_job_type job_type
);

pid_t spawn(struct sh_spawn_desc desc);

sh_run_result run(struct sh_ast_root const *root) {
    sh_run_result result = (sh_run_result) {
        .should_exit = false,
        .error_count = 0,
        .errors = NULL,
    };

    run_cmd_line(&result, &root->cmd_line);

    return result;
}

void run_cmd_line(
    sh_run_result *result,
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

void run_job_desc(sh_run_result *result, struct sh_job_desc const *job_desc) {
    // TODO: background and foreground jobs
    // TODO: piping
    struct sh_ast_job const *job = &job_desc->job;
    for (size_t idx = 0; idx < job->cmd_count; idx++) {
        run_cmd(result, &job->piped_cmds[idx], job_desc->type);
    }
}

void run_cmd(
    sh_run_result *result,
    struct sh_ast_cmd const *cmd,
    enum sh_job_type job_type
) {
    size_t argc = cmd->simple_cmd.argc;
    char **argv = cmd->simple_cmd.argv;

    // Handle `exit` builtin.
    if (argc >= 1 && strcmp(argv[0], "exit") == 0) {
        sh_exit_result exit_result = run_exit(argc, argv);

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
    };

    // TODO: write error to `result` on error
    pid_t pid = spawn(desc);
}

pid_t spawn(struct sh_spawn_desc desc) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process.

        // Handle redirection.
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
    } else if (pid != -1 && desc.job_type == SH_JOB_FG) {
        // Parent process. Only wait if the job is a foreground job.
        // Background jobs are consumed by the signal handler for `SIGCHLD` so
        // that they don't become zombie processes.
        int stat_loc;
        waitpid(pid, &stat_loc, 0);
        // TODO: what to do with `stat_loc`?
    }

    return pid;
}
