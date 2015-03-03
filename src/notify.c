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

#include "debug.h"


/* Initialize file system monitoring for given type. This function returns
 * pointer to the initialized notify structure or NULL upon error. */
struct ouroboros_notify *ouroboros_notify_init(enum ouroboros_notify_type type) {

	struct ouroboros_notify *notify;

	if ((notify = malloc(sizeof(struct ouroboros_notify))) == NULL)
		return NULL;

	notify->type = type;

	/* set defaults for directory watching */
	notify->recursive = 1;
	notify->update_nodes = 1;

	notify->include.regex = NULL;
	notify->include.size = 0;
	notify->exclude.regex = NULL;
	notify->exclude.size = 0;

	switch (type) {
	case ONT_POLL:
		break;
	case ONT_INOTIFY:
		if ((notify->s.inotify.fd = inotify_init1(IN_CLOEXEC)) == -1) {
			perror("warning: unable to initialize inotify subsystem");
			free(notify);
			return NULL;
		}
		notify->s.inotify.paths = NULL;
		notify->s.inotify.path_max = 0;
		break;
	}

	return notify;
}

/* Free allocated resources. */
void ouroboros_notify_free(struct ouroboros_notify *notify) {

	while (notify->include.size--)
		regfree(&notify->include.regex[notify->include.size]);
	free(notify->include.regex);

	while (notify->exclude.size--)
		regfree(&notify->include.regex[notify->exclude.size]);
	free(notify->exclude.regex);

	switch (notify->type) {
	case ONT_POLL:
		break;
	case ONT_INOTIFY:
		while (notify->s.inotify.path_max--)
			free(notify->s.inotify.paths[notify->s.inotify.path_max]);
		free(notify->s.inotify.paths);
		close(notify->s.inotify.fd);
		break;
	}

	free(notify);
}

/* Internal function for regex patterns compilation. On success this
 * function returns the number of compiled patterns. */
static int _compile_regex(regex_t **array, char **values) {

	int size = 0;
	int i, j;

	/* count the number of patterns */
	if (values) {
		char **ptr = values;
		while (*ptr) {
			size++;
			ptr++;
		}
	}

	/* allocate memory for all given patters, even though some
	 * of them might be invalid - we will discard them later */
	if ((*array = malloc(sizeof(regex_t) * size)) == NULL)
		return 0;

	for (i = j = 0; i < size; i++, j++) {
		int rv = regcomp(&(*array)[j], values[i], REG_EXTENDED | REG_NOSUB);
		if (rv != 0) {
			/* report compilation error and discard current pattern */
			int len = regerror(rv, &(*array)[j], NULL, 0);
			char *msg = malloc(len);
			regerror(rv, &(*array)[j], msg, len);
			fprintf(stderr, "warning: invalid pattern '%s': %s\n", values[i], msg);
			free(msg);
			j--;
		}
	}

	return j;
}

/* Enable or disable recursive directory scanning upon adding new nodes to
 * the monitoring subsystem. This function returns the previous value. */
int ouroboros_notify_recursive(struct ouroboros_notify *notify, int value) {
	int tmp = notify->recursive;
	notify->recursive = value;
	return tmp;
}

/* Enable or disable updating nodes upon event dispatching. This function
 * returns the previous value. */
int ouroboros_notify_update_nodes(struct ouroboros_notify *notify, int value) {
	int tmp = notify->update_nodes;
	notify->update_nodes = value;
	return tmp;
}

/* Set include pattern values. If given array is empty (passed NULL pointer
 * or first element is NULL), then accept-all regex is assumed as a sane
 * default. This function returns the number of processed patterns. */
int ouroboros_notify_include_patterns(struct ouroboros_notify *notify, char **values) {

	char *all[] = { ".*", NULL };

	/* set accept-all regex as a default for include pattern */
	if (values == NULL || *values == NULL)
		values = all;

	notify->include.size = _compile_regex(&notify->include.regex, values);
	return notify->include.size;
}

/* Set exclude pattern values. This function returns the number of
 * successfully processed patterns. */
int ouroboros_notify_exclude_patterns(struct ouroboros_notify *notify, char **values) {
	notify->exclude.size = _compile_regex(&notify->exclude.regex, values);
	return notify->exclude.size;
}

/* Recursively add directories into the notify monitoring subsystem. If
 * given directory array is empty, then current working directory is used
 * instead. */
int ouroboros_notify_watch(struct ouroboros_notify *notify, char **dirs) {

	if (dirs == NULL) {
		char cwd[128];
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			fprintf(stderr, "error: unable to get current working directory\n");
			return -1;
		}
		ouroboros_notify_watch_path(notify, cwd);
		return 0;
	}

	/* iterate over given directories */
	while (*dirs) {
		ouroboros_notify_watch_path(notify, *dirs);
		dirs++;
	}

	return 0;
}

/* Add given location with all subdirectories (if configured so) into the
 * monitoring subsystem. Upon error this function returns -1. */
int ouroboros_notify_watch_path(struct ouroboros_notify *notify, const char *path) {

	struct stat s;
	int wd;

	debug("adding new path: %s", path);
	if (stat(path, &s) == -1) {
		perror("warning: unable to stat pathname");
		return -1;
	}

	/* go into the recursive mode - if enabled */
	if (notify->recursive && S_ISDIR(s.st_mode)) {

		DIR *dir;
		struct dirent *dp;
		char *tmp;

		/* iterate over subdirectories */
		if ((dir = opendir(path)) != NULL) {
			while ((dp = readdir(dir)) != NULL) {

				/* omit special directories */
				if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
					continue;

				tmp = malloc(strlen(path) + strlen(dp->d_name) + 2);
				sprintf(tmp, "%s/%s", path, dp->d_name);

				/* if current name is a directory go recursive */
				if (stat(tmp, &s) != -1 && S_ISDIR(s.st_mode))
					ouroboros_notify_watch_path(notify, tmp);

				free(tmp);
			}
			closedir(dir);
		}
	}

	/* add file/directory to the monitoring subsystem */
	if ((wd = inotify_add_watch(notify->s.inotify.fd, path, IN_ATTRIB | IN_CREATE |
				IN_DELETE | IN_CLOSE_WRITE | IN_MOVE_SELF)) != -1) {

		if (wd >= notify->s.inotify.path_max) {
			int i;
			notify->s.inotify.paths = realloc(notify->s.inotify.paths, sizeof(char *) * (wd + 1));
			for (i = notify->s.inotify.path_max; i <= wd; i++)
				notify->s.inotify.paths[i] = NULL;
			notify->s.inotify.path_max = wd + 1;
		}

		/* store full path of watched location */
		notify->s.inotify.paths[wd] = strdup(path);

	}
	else
		perror("warning: unable to add inotify watch");

	return 0;
}

/* Dispatch notification event and optionally add new directories into the
 * monitoring subsystem. If current event matches given patterns, then this
 * function returns 1. Upon error this function returns -1. */
int ouroboros_notify_dispatch(struct ouroboros_notify *notify) {

	char buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
	struct inotify_event *e = (struct inotify_event *)buffer;
	ssize_t rlen;
	int i;

	rlen = read(notify->s.inotify.fd, e, sizeof(buffer));

	/* we need to read at least the size of the inotify event structure */
	if (rlen < (signed)sizeof(struct inotify_event)) {
		fprintf(stderr, "warning: dispatching inotify event failed\n");
		return -1;
	}

	debug("notify event: wd=%d, mask=%x, name=%s", e->wd, e->mask, e->name);

	/* update new nodes - directory created or permission changed */
	if (notify->update_nodes && e->mask & IN_ISDIR && e->mask & (IN_CREATE | IN_ATTRIB)) {
		char *tmp = malloc(strlen(notify->s.inotify.paths[e->wd]) + strlen(e->name) + 2);
		sprintf(tmp, "%s/%s", notify->s.inotify.paths[e->wd], e->name);
		ouroboros_notify_watch_path(notify, tmp);
		free(tmp);
	}

	/* check the name against include patterns, if matched, then check against
	 * exclude patterns - exclude takes precedence over include */
	for (i = 0; i < notify->include.size; i++)
		if (regexec(&notify->include.regex[i], e->name, 0, NULL, 0) == 0) {
			for (i = 0; i < notify->exclude.size; i++)
				if (regexec(&notify->exclude.regex[i], e->name, 0, NULL, 0) == 0)
					return 0;
			return 1;
		}

	return 0;
}
