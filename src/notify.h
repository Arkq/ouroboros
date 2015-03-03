/*
 * ouroboros - notify.h
 * Copyright (c) 2015 Arkadiusz Bokowy
 *
 * This file is a part of a ouroboros.
 *
 * This projected is licensed under the terms of the MIT license.
 *
 */

#ifndef __NOTIFY_H
#define __NOTIFY_H

#include <regex.h>


/* available notification types (might be OS specific) */
enum ouroboros_notify_type {
	ONT_POLL = 0,
	ONT_INOTIFY,
};

/* data structure definition for ERE patterns */
struct ouroboros_notify_patterns {
	regex_t *regex;
	int size;
};


struct ouroboros_notify_data_poll {
};

struct ouroboros_notify_data_inotify {
	/* inotify file descriptor */
	int fd;
	/* internal filenames tracking */
	char **paths;
	int path_max;
};


struct ouroboros_notify {

	/* configured notification type */
	enum ouroboros_notify_type type;

	/* scanning behavior */
	int recursive;
	int update_nodes;

	/* compiled ERE patterns */
	struct ouroboros_notify_patterns include;
	struct ouroboros_notify_patterns exclude;

	/* data storage for configured type */
	union {
		struct ouroboros_notify_data_poll poll;
		struct ouroboros_notify_data_inotify inotify;
	} s;

};


struct ouroboros_notify *ouroboros_notify_init(enum ouroboros_notify_type type);
void ouroboros_notify_free(struct ouroboros_notify *notify);

int ouroboros_notify_recursive(struct ouroboros_notify *notify, int value);
int ouroboros_notify_update_nodes(struct ouroboros_notify *notify, int value);
int ouroboros_notify_include_patterns(struct ouroboros_notify *notify, char **values);
int ouroboros_notify_exclude_patterns(struct ouroboros_notify *notify, char **values);

int ouroboros_notify_watch(struct ouroboros_notify *notify, char **dirs);
int ouroboros_notify_watch_path(struct ouroboros_notify *notify, const char *path);

int ouroboros_notify_dispatch(struct ouroboros_notify *notify);

#endif
