#include "run.h"

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

void run_cmd_line(
    sh_run_result *result,
    struct sh_ast_cmd_line const *cmd_line
);

void run_job_desc(sh_run_result *result, struct sh_job_desc const *job_desc);

void run_cmd(sh_run_result *result, struct sh_ast_cmd const *cmd);

void run_simple_cmd(
    sh_run_result *result,
    struct sh_ast_simple_cmd const *simple_cmd
);

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
        run_cmd(result, &job->piped_cmds[idx]);
    }
}

void run_cmd(sh_run_result *result, struct sh_ast_cmd const *cmd) {
    // TODO: redirection
    run_simple_cmd(result, &cmd->simple_cmd);
}

void run_simple_cmd(
    sh_run_result *result,
    struct sh_ast_simple_cmd const *simple_cmd
) {
    pid_t pid = fork();

    // Failed to fork.
    // Let the caller decide what to do.
    if (pid == -1) {
        // TODO: write error to `result`
        return;
    }

    // Child process.
    if (pid == 0) {
        execvp(simple_cmd->argv[0], simple_cmd->argv);

        // TODO: write error to `result`

        // This point is only reached if `execl` failed.
        // There is no point keeping the child process around, so we just exit
        // from the child process.
        exit(EXIT_FAILURE);
    }

    // Parent process.
    int stat_loc;
    waitpid(pid, &stat_loc, 0);

    // TODO: what to do with `stat_loc`?
}
