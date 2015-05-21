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
#include <string.h>
#include <proc/readproc.h>
#include <sys/wait.h>

#include "debug.h"


/* Initialize process structure with default values. */
void ouroboros_process_init(struct ouroboros_process *process,
		const char *file, char **argv) {

	process->pid = 0;
	process->file = file;
	process->argv = argv;
	process->signal = SIGTERM;

	if (pipe(process->stdinfd) == -1)
		perror("warning: unable to create pipe");

}

/* Free allocated resources. */
void ouroboros_process_free(struct ouroboros_process *process) {
	close(process->stdinfd[0]);
	close(process->stdinfd[1]);
}

/* Kill running instance of watched process. */
void kill_ouroboros_process(struct ouroboros_process *process) {

	if (process->pid <= 0)
		return;

	PROCTAB *proc;
	proc_t proc_info;

	pid_t ouroboros_pid = getpid();
	pid_t process_gpid = getpgid(process->pid);

	proc = openproc(PROC_FILLSTAT);
	memset(&proc_info, 0, sizeof(proc_info));

	while (readproc(proc, &proc_info) != NULL) {

		/* we are only interested in processes inside the process
		 * group of our supervised process */
		if (proc_info.pgrp != process_gpid)
			continue;

		/* do not become suicidal, it is not cool */
		if (proc_info.tgid == ouroboros_pid)
			continue;

		debug("killing: pid=%d, signal=%d", proc_info.tgid, process->signal);
		if (kill(proc_info.tgid, process->signal) == -1)
			perror("warning: unable to kill process");

		/* prevent zombie apocalypse */
		waitpid(proc_info.tgid, &process->status, 0);
	}

	closeproc(proc);
}

int start_ouroboros_process(struct ouroboros_process *process) {

	if ((process->pid = fork()) == -1)
		return -1;
	if (process->pid != 0) {
		debug("starting: pid=%d, cmd=%s", process->pid, process->file);
		return 0;
	}

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
