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


/* Initialize inotify monitoring subsystem. Upon error this function
 * returns -1. */
int ouroboros_notify_init(struct ouroboros_notify *notify) {

	/* first things first - be sure that *_free will work */
	notify->pattern_include = NULL;
	notify->pattern_exclude = NULL;
	notify->inclpatt_length = 0;
	notify->exclpatt_length = 0;

	/* set defaults for directory watching */
	notify->recursive = 1;
	notify->update_nodes = 1;

	if ((notify->fd = inotify_init1(IN_CLOEXEC)) == -1) {
		perror("warning: unable to initialize inotify subsystem");
		return -1;
	}

	return 0;
}

/* Free allocated resources. */
void ouroboros_notify_free(struct ouroboros_notify *notify) {

	int i;

	for (i = 0; i < notify->inclpatt_length; i++)
		regfree(&notify->pattern_include[i]);
	for (i = 0; i < notify->exclpatt_length; i++)
		regfree(&notify->pattern_include[i]);

	free(notify->pattern_include);
	free(notify->pattern_exclude);

	close(notify->fd);
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
	*array = malloc(sizeof(regex_t) * size);

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

/* Set include pattern values. If given array is empty (passed NULL pointer
 * or first element is NULL), then accept-all regexp is assumed as a sane
 * default. This function returns the number of processed patterns. */
int ouroboros_notify_include_patterns(struct ouroboros_notify *notify, char **values) {

	char *all[] = { ".*", NULL };

	/* set accept-all regexp as a default for include pattern */
	if (values == NULL || *values == NULL)
		values = all;

	notify->inclpatt_length = _compile_regex(&notify->pattern_include, values);
	return notify->inclpatt_length;
}

/* Set exclude pattern values. This function returns the number of
 * successfully processed patterns. */
int ouroboros_notify_exclude_patterns(struct ouroboros_notify *notify, char **values) {
	notify->exclpatt_length = _compile_regex(&notify->pattern_exclude, values);
	return notify->exclpatt_length;
}

/* Enable or disable recursive directory scanning upon adding new nodes to
 * the monitoring subsystem. This function returns previous value. */
int ouroboros_notify_recursive(struct ouroboros_notify *notify, int value) {
	int tmp = notify->recursive;
	notify->recursive = value;
	return tmp;
}

/* Enable or disable updating nodes upon event dispatching. This function
 * returns previous value. */
int ouroboros_notify_update_nodes(struct ouroboros_notify *notify, int value) {
	int tmp = notify->update_nodes;
	notify->update_nodes = value;
	return tmp;
}

/* Add given location with all subdirectories into the inotify monitoring
 * subsystem. Upon error this function returns -1. */
int ouroboros_notify_watch(struct ouroboros_notify *notify, const char *path) {

	struct stat s;

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

/* Dispatch notification event and optionally add new directories into the
 * monitoring subsystem. If current event matches given patterns, then this
 * function returns 1. Upon error this function returns -1. */
int ouroboros_notify_dispatch(struct ouroboros_notify *notify) {

	char buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
	struct inotify_event *e = (struct inotify_event *)buffer;
	ssize_t rlen;
	int i;

	rlen = read(notify->fd, e, sizeof(buffer));

	/* we need to read at least the size of the inotify event structure */
	if (rlen < sizeof(struct inotify_event)) {
		fprintf(stderr, "warning: dispatching inotify event failed\n");
		return -1;
	}

	debug("notify event: wd=%d, mask=%x, name=%s", e->wd, e->mask, e->name);

	/* update new nodes - directory created or permission changed */
	if (notify->update_nodes && e->mask & IN_ISDIR && e->mask & (IN_CREATE | IN_ATTRIB)) {
		debug("add new node: not implemented yet...");
	}

	/* check the name against include patterns, if matched, then check against
	 * exclude patterns - exclude takes precedence over include */
	for (i = 0; i < notify->inclpatt_length; i++)
		if (regexec(&notify->pattern_include[i], e->name, 0, NULL, 0) == 0) {
			for (i = 0; i < notify->exclpatt_length; i++)
				if (regexec(&notify->pattern_exclude[i], e->name, 0, NULL, 0) == 0)
					return 0;
			return 1;
		}

	return 0;
}
