#include "run.h"

#include <sys/wait.h>
#include <unistd.h>

int run_command_line(struct sh_ast const *command_line);
int run_job(struct sh_job const *job);
int run_command(struct sh_ast const *command);
int run_simple_command(struct sh_ast const *simple_command);

int run(struct sh_ast const *root) {
    return run_command_line(root->data.root.command_line);
}

int run_command_line(struct sh_ast const *command_line) {
    // Empty line — nothing to run.
    if (command_line == NULL) {
        return 0;
    }

    if (command_line->type == SH_COMMAND_REPEAT) {
        // TODO: repeat command from history
        return 0;
    }

    for (size_t idx = 0; idx < command_line->data.command_line.job_count; idx++)
    {
        run_job(&command_line->data.command_line.jobs[idx]);
    }

    return 0;
}

int run_job(struct sh_job const *job) {
    // TODO: background and foreground jobs
    // TODO: piping
    struct sh_ast *job_node = job->ast_node;
    for (size_t idx = 0; idx < job_node->data.job.cmd_count; idx++) {
        run_command(job_node->data.job.piped_cmds[idx]);
    }

    return 0;
}

int run_command(struct sh_ast const *command) {
    // TODO: redirection
    return run_simple_command(command->data.command.simple_command);
}

int run_simple_command(struct sh_ast const *simple_command) {
    pid_t pid = fork();

    // Failed to fork.
    // Let the caller decide what to do.
    if (pid == -1) {
        return -1;
    }

    // Child process.
    if (pid == 0) {
        execvp(
            simple_command->data.simple_command.argv[0],
            simple_command->data.simple_command.argv
        );

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
