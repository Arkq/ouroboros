/*
 * ouroboros - config.h
 * Copyright (c) 2015 Arkadiusz Bokowy
 *
 * This file is a part of a ouroboros.
 *
 * This projected is licensed under the terms of the MIT license.
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H


/* key definitions for configuration file */
#define OOBSCONF_APP_FILENAME "filename"
#define OOBSCONF_WATCH_PATH "watch-path"
#define OOBSCONF_WATCH_RECURSIVE "watch-recursive"
#define OOBSCONF_WATCH_UPDATE_NODES "watch-update-nodes"
#define OOBSCONF_WATCH_INCLUDE "watch-include"
#define OOBSCONF_WATCH_EXCLUDE "watch-exclude"
#define OOBSCONF_KILL_LATENCY "kill-latency"
#define OOBSCONF_KILL_SIGNAL "kill-signal"
#define OOBSCONF_REDIRECT_INPUT "redirect-input"
#define OOBSCONF_REDIRECT_OUTPUT "redirect-output"
#define OOBSCONF_REDIRECT_SIGNAL "redirect-signal"


struct ouroboros_config {

	/* inotify notification */
	int watch_recursive;
	int watch_update_nodes;
	char **watch_paths;
	char **watch_includes;
	char **watch_excludes;

	/* kill and reload */
	double kill_latency;
	int kill_signal;

	/* IO redirection */
	int redirect_input;
	char *redirect_output;
	int *redirect_signals;

};


void ouroboros_config_init(struct ouroboros_config *config);
void ouroboros_config_free(struct ouroboros_config *config);

int load_ouroboros_config(const char *filename, const char *appname,
		struct ouroboros_config *config);

int ouroboros_config_add_int(int **array, int value);
int ouroboros_config_add_string(char ***array, const char *value);

int ouroboros_config_get_bool(const char *name);
int ouroboros_config_get_signal(const char *name);

char *get_ouroboros_config_file(void);

#endif
