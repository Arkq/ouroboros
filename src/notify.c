/*
 * ouroboros - notify.c
 * Copyright (c) 2015 Arkadiusz Bokowy
 *
 * This file is a part of a ouroboros.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include "notify.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#if HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif

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
	notify->dirs_only = 0;
	notify->files_only = 0;

	notify->include.regex = NULL;
	notify->include.size = 0;
	notify->exclude.regex = NULL;
	notify->exclude.size = 0;

	notify->paths = NULL;

	switch (type) {
	case ONT_POLL:
		notify->s.poll.watched = NULL;
		notify->s.poll.size = 0;
		break;
#if HAVE_SYS_INOTIFY_H
	case ONT_INOTIFY:
		if ((notify->s.inotify.fd = inotify_init1(IN_CLOEXEC)) == -1) {
			perror("warning: unable to initialize inotify subsystem");
			free(notify);
			return NULL;
		}
		notify->s.inotify.watched = NULL;
		notify->s.inotify.size = 0;
		break;
#endif /* HAVE_SYS_INOTIFY_H */
	}

	return notify;
}

/* Free allocated resources. */
void ouroboros_notify_free(struct ouroboros_notify *notify) {

	while (notify->include.size--)
		regfree(&notify->include.regex[notify->include.size]);
	free(notify->include.regex);

	while (notify->exclude.size--)
		regfree(&notify->exclude.regex[notify->exclude.size]);
	free(notify->exclude.regex);

	if (notify->paths) {
		char **ptr = notify->paths;
		while (*ptr) {
			free(*ptr);
			ptr++;
		}
	}
	free(notify->paths);

	switch (notify->type) {
	case ONT_POLL:
		while (notify->s.poll.size--)
			free(notify->s.poll.watched[notify->s.poll.size].path);
		free(notify->s.poll.watched);
		break;
#if HAVE_SYS_INOTIFY_H
	case ONT_INOTIFY:
		while (notify->s.inotify.size--)
			free(notify->s.inotify.watched[notify->s.inotify.size].path);
		free(notify->s.inotify.watched);
		close(notify->s.inotify.fd);
		break;
#endif /* HAVE_SYS_INOTIFY_H */
	}

	free(notify);
}

/* Internal function for regex patterns compilation. On success this
 * function returns the number of compiled patterns. */
static int _compile_regex(regex_t **array, char **values) {

	int size = 0;
	int i, j;

	/* count the number of patterns */
	if (values)
		while (values[size])
			size++;

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

/* Enable or disable watching directories only. This scanning behavior might
 * boost performance significantly, however may not work on all file systems.
 * This function returns the previous value. */
int ouroboros_notify_dirs_only(struct ouroboros_notify *notify, int value) {
	int tmp = notify->dirs_only;
	notify->dirs_only = value;
	return tmp;
}

/* Enable or disable watching files only. This scanning behavior might boost
 * performance and omit false positive reloads. This function returns the
 * previous value. */
int ouroboros_notify_files_only(struct ouroboros_notify *notify, int value) {
	int tmp = notify->files_only;
	notify->files_only = value;
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

	char cwd[128];
	char *defaults[] = { cwd, NULL };
	int size = 0;

	if (dirs == NULL) {
		dirs = defaults;
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			fprintf(stderr, "error: unable to get current working directory\n");
			return -1;
		}
	}

	/* get the number of elements in the array */
	while (dirs[size])
		size++;

	if ((notify->paths = malloc(sizeof(char *) * (size + 1))) == NULL)
		return -1;

	/* list terminator */
	notify->paths[size] = NULL;

	/* iterate over given directories */
	while (size--) {
		ouroboros_notify_watch_path(notify, dirs[size]);
		notify->paths[size] = strdup(dirs[size]);
	}

	return 0;
}

/* Internal function to check name against the regex patterns. If given
 * name should trigger notification 1 is returned, otherwise 0. */
static int _check_patterns(struct ouroboros_notify *notify, const char *name) {

	int i;

	/* check the name against include patterns, if matched, then check against
	 * exclude patterns - exclude takes precedence over include */
	for (i = notify->include.size; i--; )
		if (regexec(&notify->include.regex[i], name, 0, NULL, 0) == 0) {
			for (i = notify->exclude.size; i--; )
				if (regexec(&notify->exclude.regex[i], name, 0, NULL, 0) == 0)
					return 0;
			return 1;
		}

	return 0;
}

/* Internal function to add new path to the monitoring pool. On success this
 * function returns 0, otherwise -1. */
static int _poll_add_path(struct ouroboros_notify_data_poll *data,
		const char *path, const struct timespec *mtime) {

	int end = data->size;

	data->size++;
	data->watched = realloc(data->watched, sizeof(*data->watched) * data->size);
	data->watched[end].mtime.tv_sec = mtime->tv_sec;
	data->watched[end].mtime.tv_nsec = mtime->tv_nsec;
	data->watched[end].path = strdup(path);

	return 0;
}

/* Add given location with all subdirectories (if configured so) into the
 * monitoring subsystem. Upon error this function returns -1. */
int ouroboros_notify_watch_path(struct ouroboros_notify *notify, const char *path) {
	debug("adding new path: %s", path);

	struct stat s;

	if (stat(path, &s) == -1) {
		perror("warning: unable to stat pathname");
		return -1;
	}

	/* add current path to the monitoring pool */
	if (notify->type == ONT_POLL)
		if (!(notify->files_only && S_ISDIR(s.st_mode)))
			if (_check_patterns(notify, path))
				_poll_add_path(&notify->s.poll, path, &s.st_mtim);

	/* iterate over all nodes if path is a directory */
	if (S_ISDIR(s.st_mode) && (notify->recursive || notify->type == ONT_POLL)) {

		DIR *dir;
		struct dirent *dp;
		char *tmp;

		if ((dir = opendir(path)) != NULL) {
			while ((dp = readdir(dir)) != NULL) {

				/* omit special directories */
				if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
					continue;

				tmp = malloc(strlen(path) + strlen(dp->d_name) + 2);
				sprintf(tmp, "%s/%s", path, dp->d_name);

				if (stat(tmp, &s) != -1) {
					if (S_ISDIR(s.st_mode)) {

						/* recursive mode, so go deeper */
						if (notify->recursive)
							ouroboros_notify_watch_path(notify, tmp);

					}
					else {

						/* add node to the monitoring pool */
						if (notify->type == ONT_POLL && !notify->dirs_only)
							if (_check_patterns(notify, tmp))
								_poll_add_path(&notify->s.poll, tmp, &s.st_mtim);

					}
				}

				free(tmp);
			}
			closedir(dir);
		}
	}

	switch (notify->type) {
	case ONT_POLL:
		/* Poll-based notification type is incorporated in the main code base of
		 * this function. It has some penalty on the readability. Nonetheless it
		 * is the only reliable method - always available - so it is wise to put
		 * some micro optimization into this section. */
		break;
#if HAVE_SYS_INOTIFY_H
	case ONT_INOTIFY:
		{
			int wd;
			int i;

			/* add path to the monitoring subsystem */
			if ((wd = inotify_add_watch(notify->s.inotify.fd, path, IN_ATTRIB |
							IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVE_SELF)) == -1)
				perror("warning: unable to add inotify watch");
			else {

				/* check for already stored watch descriptor */
				for (i = notify->s.inotify.size; i--; )
					if (notify->s.inotify.watched[i].wd == wd)
						break;

				/* add new watched location (full patch) */
				if (i == -1) {
					notify->s.inotify.size++;
					notify->s.inotify.watched = realloc(notify->s.inotify.watched,
							sizeof(*notify->s.inotify.watched) * notify->s.inotify.size);
					notify->s.inotify.watched[notify->s.inotify.size - 1].wd = wd;
					notify->s.inotify.watched[notify->s.inotify.size - 1].path = strdup(path);
				}

			}
		}
		break;
#endif /* HAVE_SYS_INOTIFY_H */
	}

	return 0;
}

/* Dispatch notification event and optionally add new directories into the
 * monitoring subsystem. If current event matches given patterns, then this
 * function returns 1. Upon error this function returns -1. */
int ouroboros_notify_dispatch(struct ouroboros_notify *notify) {
	debug("dispatch");

	switch (notify->type) {
	case ONT_POLL:
		{
			if (notify->update_nodes) {

				struct ouroboros_notify_data_poll olddata;
				char **dirs = notify->paths;
				int rv = 0;

				/* save current data snapshot and clear original */
				memcpy(&olddata, &notify->s.poll, sizeof(olddata));
				notify->s.poll.watched = NULL;
				notify->s.poll.size = 0;

				/* get fresh data snapshot */
				while (*dirs) {
					ouroboros_notify_watch_path(notify, *dirs);
					dirs++;
				}

				if (olddata.size != notify->s.poll.size)
					rv = 1;
				else
					/* this logic exploits the fact, that the order of returned nodes
					 * from the stream is constant - if FS has not been modified */
					while (olddata.size) {
						olddata.size--;
						free(olddata.watched[olddata.size].path);
						/* check if file time-stamp has changed */
						if (notify->s.poll.watched[olddata.size].mtime.tv_sec != olddata.watched[olddata.size].mtime.tv_sec ||
								notify->s.poll.watched[olddata.size].mtime.tv_nsec != olddata.watched[olddata.size].mtime.tv_nsec) {
							rv = 1;
							break;
						}
					}

				while (olddata.size--)
					free(olddata.watched[olddata.size].path);
				free(olddata.watched);

				return rv;
			}
			else {

				struct stat s;
				int rv = 0;
				int i;

				for (i = notify->s.poll.size; i--; ) {
					if (stat(notify->s.poll.watched[i].path, &s) == -1)
						/* the most probable reason for this fail is that the file has
						 * been removed, however we are working in the non-update mode,
						 * so drop this error silently */
						continue;
					/* check if file time-stamp has changed */
					if (notify->s.poll.watched[i].mtime.tv_sec != s.st_mtim.tv_sec ||
							notify->s.poll.watched[i].mtime.tv_nsec != s.st_mtim.tv_nsec) {
						notify->s.poll.watched[i].mtime.tv_sec = s.st_mtim.tv_sec;
						notify->s.poll.watched[i].mtime.tv_nsec = s.st_mtim.tv_nsec;
						rv = 1;
					}
				}

				return rv;
			}
		}
		break;
#if HAVE_SYS_INOTIFY_H
	case ONT_INOTIFY:
		{
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

				/* get the index of watch descriptor from the current event */
				i = notify->s.inotify.size;
				while (i && notify->s.inotify.watched[--i].wd != e->wd)
					continue;

				char *tmp = malloc(strlen(notify->s.inotify.watched[i].path) + strlen(e->name) + 2);
				sprintf(tmp, "%s/%s", notify->s.inotify.watched[i].path, e->name);
				ouroboros_notify_watch_path(notify, tmp);
				free(tmp);

			}
			/* delete node - it seems that the path has been deleted */
			else if (e->mask & IN_IGNORED) {

				/* get the index of watch descriptor from the current event */
				i = notify->s.inotify.size;
				while (i && notify->s.inotify.watched[--i].wd != e->wd)
					continue;

				notify->s.inotify.size--;
				free(notify->s.inotify.watched[i].path);
				if (notify->s.inotify.size)
					memcpy(&notify->s.inotify.watched[i],
							&notify->s.inotify.watched[notify->s.inotify.size],
							sizeof(*notify->s.inotify.watched));

			}

			return _check_patterns(notify, e->name);
		}
		break;
#endif /* HAVE_SYS_INOTIFY_H */
	}

	return 0;
}
