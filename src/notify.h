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


struct ouroboros_notify {

	int fd;

	/* compiled ERE patterns */
	regex_t *pattern_include;
	regex_t *pattern_exclude;
	int inclpatt_length;
	int exclpatt_length;

};


void ouroboros_notify_init(struct ouroboros_notify *notify,
		char **include, char **exclude);
void ouroboros_notify_free(struct ouroboros_notify *notify);
int ouroboros_notify_dispatch(struct ouroboros_notify *notify);
int ouroboros_notify_watch(struct ouroboros_notify *notify, const char *path);
int ouroboros_notify_watch_directories(struct ouroboros_notify *notify,
		char *const *dirs);

#endif
