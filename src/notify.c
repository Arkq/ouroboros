/*
 * ouroboros - notify.c
 * Copyright (c) 2015 Arkadiusz Bokowy
 *
 * This file is a part of a ouroboros.
 *
 * This projected is licensed under the terms of the MIT license.
 *
 */

#include "notify.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>


/* Initialize inotify monitoring subsystem. */
void ouroboros_notify_init(struct ouroboros_notify *notify) {
	notify->fd = inotify_init1(IN_CLOEXEC);
}

/* Free allocated resources. */
void ouroboros_notify_free(struct ouroboros_notify *notify) {
	close(notify->fd);
}

/* Dispatch notification event and optionally add new directories into the
 * monitoring subsystem. Upon error this function returns -1. */
int ouroboros_notify_dispatch(struct ouroboros_notify *notify) {

	char buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
	struct inotify_event *e = (struct inotify_event *)buffer;
	ssize_t rlen;

	rlen = read(notify->fd, e, sizeof(buffer));

	return 0;
}

/* Add given location with all subdirectories into the inotify monitoring
 * subsystem. Upon error this function returns -1. */
int ouroboros_notify_watch(struct ouroboros_notify *notify, const char *path) {

	struct stat s;

	if (stat(path, &s) == -1) {
		perror("warning: unable to stat pathname");
		return -1;
	}

	if (S_ISDIR(s.st_mode)) {

		DIR *dir;
		struct dirent *dp;
		char *tmp;

		/* iterate over subdirectories */
		if ((dir = opendir(path)) != NULL) {
			while ((dp = readdir(dir)) != NULL) {
				tmp = malloc(strlen(path) + strlen(dp->d_name) + 2);
				sprintf(tmp, "%s/%s", path, dp->d_name);

				/* if current name is a directory go recursive */
				if (stat(tmp, &s) != -1 && S_ISDIR(s.st_mode))
					ouroboros_notify_watch(notify, tmp);

				free(tmp);
			}
			closedir(dir);
		}
	}

	/* add file/directory to the monitoring subsystem */
	if (inotify_add_watch(notify->fd, path, IN_ATTRIB | IN_CREATE |
				IN_DELETE | IN_MODIFY | IN_MOVE_SELF) == -1)
		perror("warning: unable to add inotify watch");

	return 0;
}

/* Recursively add directories into the inotify monitoring subsystem. If
 * given directory array is empty, then current working directory is used
 * instead. */
int ouroboros_notify_watch_directories(struct ouroboros_notify *notify,
		char *const *dirs) {

	if (dirs == NULL) {
		char cwd[128];
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			fprintf(stderr, "error: unable to get current working directory\n");
			return -1;
		}
		ouroboros_notify_watch(notify, cwd);
		return 0;
	}

	/* iterate over given directories */
	while (*dirs) {
		ouroboros_notify_watch(notify, *dirs);
		dirs++;
	}

	return 0;
}
