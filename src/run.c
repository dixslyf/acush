#include "run.h"

#include <sys/wait.h>
#include <unistd.h>

int run_cmd_line(struct sh_ast_cmd_line const *cmd_line);
int run_job_desc(struct sh_job_desc const *job_desc);
int run_cmd(struct sh_ast_cmd const *cmd);
int run_simple_cmd(struct sh_ast_simple_cmd const *simple_cmd);

int run(struct sh_ast_root const *root) {
    return run_cmd_line(&root->cmd_line);
}

int run_cmd_line(struct sh_ast_cmd_line const *cmd_line) {
    // Empty line — nothing to run.
    if (cmd_line == NULL) {
        return 0;
    }

    if (cmd_line->type == SH_COMMAND_REPEAT) {
        // TODO: repeat command from history
        return 0;
    }

    for (size_t idx = 0; idx < cmd_line->job_count; idx++) {
        run_job_desc(&cmd_line->job_descs[idx]);
    }

    return 0;
}

int run_job_desc(struct sh_job_desc const *job_desc) {
    // TODO: background and foreground jobs
    // TODO: piping
    struct sh_ast_job const *job = &job_desc->job;
    for (size_t idx = 0; idx < job->cmd_count; idx++) {
        run_cmd(&job->piped_cmds[idx]);
    }

    return 0;
}

int run_cmd(struct sh_ast_cmd const *cmd) {
    // TODO: redirection
    return run_simple_cmd(&cmd->simple_cmd);
}

int run_simple_cmd(struct sh_ast_simple_cmd const *simple_cmd) {
    pid_t pid = fork();

    // Failed to fork.
    // Let the caller decide what to do.
    if (pid == -1) {
        return -1;
    }

    // Child process.
    if (pid == 0) {
        execvp(simple_cmd->argv[0], simple_cmd->argv);

        // TODO: error handling

        // This point is only reached if `execl` failed.
        // There is no point keeping the child process around, so we just exit
        // from the child process.
        exit(EXIT_FAILURE);
    }

    // Parent process.
    int stat_loc;
    waitpid(pid, &stat_loc, 0);

    // TODO: what to do with `stat_loc`?

    return pid;
}
