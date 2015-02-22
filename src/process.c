/*
 * ouroboros - process.c
 * Copyright (c) 2015 Arkadiusz Bokowy
 *
 * This file is a part of a ouroboros.
 *
 * This projected is licensed under the terms of the MIT license.
 *
 */

#include "process.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>


/* Initialize process structure with default values. */
void ouroboros_process_init(struct ouroboros_process *process,
		const char *file, char **argv) {

	process->pid = 0;
	process->file = file;
	process->argv = argv;
	process->signal = SIGTERM;

	pipe(process->stdinfd);

}

/* Free allocated resources. */
void ouroboros_process_free(struct ouroboros_process *process) {
	close(process->stdinfd[0]);
	close(process->stdinfd[1]);
}

/* Kill running instance of watched process. */
void kill_ouroboros_process(struct ouroboros_process *process) {
	if (process->pid > 0) {
		if (kill(process->pid, process->signal) == -1)
			perror("warning: unable to kill process");
		waitpid(process->pid, &process->status, 0);
	}
}

int start_ouroboros_process(struct ouroboros_process *process) {

	if ((process->pid = fork()) == -1)
		return -1;
	if (process->pid != 0)
		return 0;

	/* setup IO redirections */
	dup2(process->stdinfd[0], fileno(stdin));
	if (process->output) {
		if (freopen(process->output, "w", stdout) != NULL)
			dup2(fileno(stdout), fileno(stderr));
		else
			perror("warning: unable to redirect output");
	}

	/* close temporal file descriptors */
	close(process->stdinfd[0]);
	close(process->stdinfd[1]);

	execvp(process->file, process->argv);
	perror("error: unable to exec process");
	exit(EXIT_FAILURE);
}
