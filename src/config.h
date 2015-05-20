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

#include "notify.h"


/* key definitions for configuration file */
#define OCKD_APP_FILENAME "filename"
#define OCKD_WATCH_ENGINE "watch-engine"
#define OCKD_WATCH_PATH "watch-path"
#define OCKD_WATCH_RECURSIVE "watch-recursive"
#define OCKD_WATCH_UPDATE_NODES "watch-update-nodes"
#define OCKD_WATCH_INCLUDE "watch-include"
#define OCKD_WATCH_EXCLUDE "watch-exclude"
#define OCKD_WATCH_DIR_ONLY "watch-dirs-only"
#define OCKD_WATCH_FILE_ONLY "watch-files-only"
#define OCKD_KILL_SIGNAL "kill-signal"
#define OCKD_KILL_LATENCY "kill-latency"
#define OCKD_START_LATENCY "start-latency"
#define OCKD_REDIRECT_INPUT "redirect-input"
#define OCKD_REDIRECT_OUTPUT "redirect-output"
#define OCKD_REDIRECT_SIGNAL "redirect-signal"
#define OCKD_SERVER_INTERFACE "server-interface"
#define OCKD_SERVER_PORT "server-port"


struct ouroboros_config {

	/* notification type - engine */
	enum ouroboros_notify_type engine;

	/* notification */
	int watch_recursive;
	int watch_update_nodes;
	int watch_dirs_only;
	int watch_files_only;
	char **watch_paths;
	char **watch_includes;
	char **watch_excludes;

	/* kill and reload */
	int kill_signal;
	double kill_latency;
	double start_latency;

	/* IO redirection */
	int redirect_input;
	char *redirect_output;
	int *redirect_signals;

	/* server binding */
	char *server_iface;
	int server_port;

};


void ouroboros_config_init(struct ouroboros_config *config);
void ouroboros_config_free(struct ouroboros_config *config);

int load_ouroboros_config(const char *filename, const char *appname,
		struct ouroboros_config *config);

int ouroboros_config_add_int(int **array, int value);
int ouroboros_config_add_string(char ***array, const char *value);

int ouroboros_config_get_bool(const char *name);
int ouroboros_config_get_engine(const char *name);
int ouroboros_config_get_signal(const char *name);

char *get_ouroboros_config_file(void);

#endif
