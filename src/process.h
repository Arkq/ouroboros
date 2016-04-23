/*
 * ouroboros - process.h
 * Copyright (c) 2015 Arkadiusz Bokowy
 *
 * This file is a part of a ouroboros.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef __PROCESS_H
#define __PROCESS_H

#include <unistd.h>


struct ouroboros_process {

	/* process creation */
	pid_t pid;
	const char *file;
	char **argv;

	/* process destruction */
	int signal;
	int status;

	/* IO redirections */
	int stdinfd[2];
	char *output;

};


void ouroboros_process_init(struct ouroboros_process *process,
		const char *file, char *argv[]);
void ouroboros_process_free(struct ouroboros_process *process);
void kill_ouroboros_process(struct ouroboros_process *process);
int start_ouroboros_process(struct ouroboros_process *process);

#endif
