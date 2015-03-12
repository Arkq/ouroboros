/*
 * ouroboros - config.c
 * Copyright (c) 2015 Arkadiusz Bokowy
 *
 * This file is a part of a ouroboros.
 *
 * This projected is licensed under the terms of the MIT license.
 *
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "config.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if ENABLE_LIBCONFIG
#include <libconfig.h>
#endif


/* Initialize configuration structure with default values. */
void ouroboros_config_init(struct ouroboros_config *config) {

	/* fall-back engine - will always work */
	config->engine = ONT_POLL;

	config->watch_recursive = 0;
	config->watch_update_nodes = 0;
	config->watch_dirs_only = 0;
	config->watch_files_only = 0;
	config->watch_paths = NULL;
	config->watch_includes = NULL;
	config->watch_excludes = NULL;

	config->kill_latency = 1;
	config->kill_signal = SIGTERM;

	config->redirect_input = 0;
	config->redirect_output = NULL;
	config->redirect_signals = NULL;

#if ENABLE_SERVER
	config->server_iface = NULL;
	config->server_port = 3945;
#endif /* ENABLE_SERVER */

}

/* Internal function which actually frees array resources. */
static void _free_array(char ***array) {
	if (*array) {
		char **ptr = *array;
		while (*ptr) {
			free(*ptr);
			ptr++;
		}
		free(*array);
		*array = NULL;
	}
}

/* Free allocated resources. */
void ouroboros_config_free(struct ouroboros_config *config) {
	_free_array(&config->watch_paths);
	_free_array(&config->watch_includes);
	_free_array(&config->watch_excludes);
	free(config->redirect_output);
	free(config->redirect_signals);
#if ENABLE_SERVER
	free(config->server_iface);
#endif
}

#if ENABLE_LIBCONFIG
/* Internal function which actually reads data from the configuration file. */
static void _load_config(const config_setting_t *root, struct ouroboros_config *config) {

	config_setting_t *array;
	const char *tmp;
	int length;
	int val;
	int i;

	if (config_setting_lookup_string(root, OCKD_WATCH_ENGINE, &tmp))
		if ((val = ouroboros_config_get_engine(tmp)) != -1)
			config->engine = val;

	config_setting_lookup_bool(root, OCKD_WATCH_RECURSIVE, &config->watch_recursive);

	config_setting_lookup_bool(root, OCKD_WATCH_UPDATE_NODES, &config->watch_update_nodes);

	if ((array = config_setting_get_member(root, OCKD_WATCH_PATH)) != NULL) {
		_free_array(&config->watch_paths);
		length = config_setting_length(array);
		for (i = 0; i < length; i++)
			if ((tmp = config_setting_get_string_elem(array, i)) != NULL)
				ouroboros_config_add_string(&config->watch_paths, tmp);
	}

	if ((array = config_setting_get_member(root, OCKD_WATCH_INCLUDE)) != NULL) {
		_free_array(&config->watch_includes);
		length = config_setting_length(array);
		for (i = 0; i < length; i++)
			if ((tmp = config_setting_get_string_elem(array, i)) != NULL)
				ouroboros_config_add_string(&config->watch_includes, tmp);
	}

	if ((array = config_setting_get_member(root, OCKD_WATCH_EXCLUDE)) != NULL) {
		_free_array(&config->watch_excludes);
		length = config_setting_length(array);
		for (i = 0; i < length; i++)
			if ((tmp = config_setting_get_string_elem(array, i)) != NULL)
				ouroboros_config_add_string(&config->watch_excludes, tmp);
	}

	config_setting_lookup_bool(root, OCKD_WATCH_DIR_ONLY, &config->watch_dirs_only);

	config_setting_lookup_bool(root, OCKD_WATCH_FILE_ONLY, &config->watch_files_only);

	config_setting_lookup_float(root, OCKD_KILL_LATENCY, &config->kill_latency);

	if (config_setting_lookup_string(root, OCKD_KILL_SIGNAL, &tmp))
		if ((val = ouroboros_config_get_signal(tmp)) != 0)
			config->kill_signal = val;

	config_setting_lookup_bool(root, OCKD_REDIRECT_INPUT, &config->redirect_input);

	/* output setting is a special one, because it can be boolean or string*/
	if (config_setting_lookup_bool(root, OCKD_REDIRECT_OUTPUT, &val)) {
		free(config->redirect_output);
		config->redirect_output = NULL;
	}
	else if (config_setting_lookup_string(root, OCKD_REDIRECT_OUTPUT, &tmp)) {
		free(config->redirect_output);
		config->redirect_output = NULL;
		if (strlen(tmp) != 0)
			config->redirect_output = strdup(tmp);
	}

	if ((array = config_setting_get_member(root, OCKD_REDIRECT_SIGNAL)) != NULL) {
		free(config->redirect_signals);
		config->redirect_signals = NULL;
		length = config_setting_length(array);
		for (i = 0; i < length; i++)
			if ((tmp = config_setting_get_string_elem(array, i)) != NULL)
				if ((val = ouroboros_config_get_signal(tmp)) != 0)
					ouroboros_config_add_int(&config->redirect_signals, val);
	}

#if ENABLE_SERVER
	if (config_setting_lookup_string(root, OCKD_SERVER_INTERFACE, &tmp)) {
		free(config->server_iface);
		config->server_iface = NULL;
		if (strcmp(tmp, "none") != 0)
			config->server_iface = strdup(tmp);
	}

	config_setting_lookup_int(root, OCKD_SERVER_PORT, &config->server_port);
#endif /* ENABLE_SERVER */

}
#endif /* ENABLE_LIBCONFIG */

#if ENABLE_LIBCONFIG
/* Load configuration from the given file. */
int load_ouroboros_config(const char *filename, const char *appname,
		struct ouroboros_config *config) {

	/* passing NULL does not trigger error */
	if (filename == NULL)
		return 0;

	config_t cfg;
	config_setting_t *root;
	const char *tmp;
	int i;

	config_init(&cfg);
	if (!config_read_file(&cfg, filename)) {
		config_destroy(&cfg);
		return -1;
	}

	config_set_auto_convert(&cfg, 1);
	root = config_root_setting(&cfg);

	/* read global configuration */
	_load_config(root, config);

	/* check it there is an extra section for our application name */
	int count = config_setting_length(root);
	for (i = 0; i < count; i++) {
		config_setting_t *extra = config_setting_get_elem(root, i);

		if (!config_setting_is_group(extra))
			continue;
		if (!config_setting_lookup_string(extra, OCKD_APP_FILENAME, &tmp))
			continue;
		if (strcmp(tmp, appname) != 0)
			continue;

		/* read extra configuration */
		_load_config(extra, config);

	}

	config_destroy(&cfg);
	return 0;
}
#endif /* ENABLE_LIBCONFIG */

/* Add new non-zero value to the array. On success this function returns
 * the number of stored elements in the array, otherwise -1. */
int ouroboros_config_add_int(int **array, int value) {

	if (array == NULL)
		return -1;

	int size = 2;

	if (*array) {
		int *ptr = *array;
		while (*ptr) {
			size++;
			ptr++;
		}
	}

	*array = realloc(*array, size * sizeof(int));
	if (*array == NULL)
		return -1;

	(*array)[size - 2] = value;
	(*array)[size - 1] = 0;
	return size - 1;
}

/* Add new value to the array. On success this function returns the number
 * of stored elements in the array, otherwise -1. */
int ouroboros_config_add_string(char ***array, const char *value) {

	if (array == NULL)
		return -1;

	int size = 2;

	if (*array) {
		/* add the number of elements currently stored in the array */
		char **ptr = *array;
		while (*ptr) {
			size++;
			ptr++;
		}
	}

	*array = realloc(*array, size * sizeof(char *));
	if (*array == NULL)
		return -1;

	(*array)[size - 2] = strdup(value);
	(*array)[size - 1] = NULL;
	return size - 1;
}

/* Convert given name (string) into the boolean value. */
int ouroboros_config_get_bool(const char *name) {
	if (atoi(name) || strcasecmp(name, "true") == 0)
		return 1;
	return 0;
}

/* Convert engine name into the corresponding enum number. If name can not
 * be resolved, then -1 is returned. */
int ouroboros_config_get_engine(const char *name) {

	if (strcmp(name, "poll") == 0)
		return ONT_POLL;
#if HAVE_SYS_INOTIFY_H
	else if (strcmp(name, "inotify") == 0)
		return ONT_INOTIFY;
#endif /* HAVE_SYS_INOTIFY_H */

	return -1;
}

/* Convert signal name into the corresponding number for current platform.
 * If name can not be found, then 0 is returned. */
int ouroboros_config_get_signal(const char *name) {

	int signal = strtol(name, NULL, 0);
	if (signal)
		return signal;

	struct {
		const char *name;
		int value;
	} data[] = {
		/* standard POSIX.1-1990 signals */
		{ "SIGHUP", SIGHUP },
		{ "SIGINT", SIGINT },
		{ "SIGQUIT", SIGQUIT },
		{ "SIGILL", SIGILL },
		{ "SIGABRT", SIGABRT },
		{ "SIGFPE", SIGFPE },
		{ "SIGKILL", SIGKILL },
		{ "SIGSEGV", SIGSEGV },
		{ "SIGPIPE", SIGPIPE },
		{ "SIGALRM", SIGALRM },
		{ "SIGTERM", SIGTERM },
		{ "SIGUSR1", SIGUSR1 },
		{ "SIGUSR2", SIGUSR2 },
		{ "SIGCHLD", SIGCHLD },
		{ "SIGCONT", SIGCONT },
		{ "SIGSTOP", SIGSTOP },
		{ "SIGTSTP", SIGTSTP },
		{ "SIGTTIN", SIGTTIN },
		{ "SIGTTOU", SIGTTOU },
		/* signals described in SUSv2 and POSIX.1-2001 */
#ifdef SIGBUS
		{ "SIGBUS", SIGBUS },
#endif
#ifdef SIGPOLL
		{ "SIGPOLL", SIGPOLL },
#endif
#ifdef SIGPROF
		{ "SIGPROF", SIGPROF },
#endif
#ifdef SIGSYS
		{ "SIGSYS", SIGSYS },
#endif
#ifdef SIGTRAP
		{ "SIGTRAP", SIGTRAP },
#endif
#ifdef SIGURG
		{ "SIGURG", SIGURG },
#endif
#ifdef SIGVTALRM
		{ "SIGVTALRM", SIGVTALRM },
#endif
#ifdef SIGXCPU
		{ "SIGXCPU", SIGXCPU },
#endif
#ifdef SIGXFSZ
		{ "SIGXFSZ", SIGXFSZ },
#endif
		/* various other signals */
#ifdef SIGIOT
		{ "SIGIOT", SIGIOT },
#endif
#ifdef SIGEMT
		{ "SIGEMT", SIGEMT },
#endif
#ifdef SIGSTKFLT
		{ "SIGSTKFLT", SIGSTKFLT },
#endif
#ifdef SIGIO
		{ "SIGIO", SIGIO },
#endif
#ifdef SIGCLD
		{ "SIGCLD", SIGCLD },
#endif
#ifdef SIGPWR
		{ "SIGPWR", SIGPWR },
#endif
#ifdef SIGINFO
		{ "SIGINFO", SIGINFO },
#endif
#ifdef SIGLOST
		{ "SIGLOST", SIGLOST },
#endif
#ifdef SIGWINCH
		{ "SIGWINCH", SIGWINCH },
#endif
#ifdef SIGUNUSED
		{ "SIGUNUSED", SIGUNUSED },
#endif
		{ NULL, 0 },
	};

	int i = 0;
	do {
		if (strcasecmp(data[i].name, name) == 0)
			return data[i].value;
	}
	while (data[++i].name != NULL);

	return 0;
}

/* This functions checks for the configuration file in the standard
 * location. On success it returns file name, otherwise NULL. */
char *get_ouroboros_config_file(void) {

	const char config[] = "ouroboros/ouroboros.conf";
	char *fullpath;
	char *tmp;

	/* get our file path in the XDG configuration directory */
	if ((tmp = getenv("XDG_CONFIG_HOME")) != NULL) {
		fullpath = malloc(strlen(tmp) + 1 + sizeof(config));
		sprintf(fullpath, "%s/%s", tmp, config);
	}
	else {
		tmp = getenv("HOME");
		fullpath = malloc(strlen(tmp) + 9 + sizeof(config));
		sprintf(fullpath, "%s/.config/%s", tmp, config);
	}

	/* check if file exists */
	if (access(fullpath, F_OK) == -1) {
		free(fullpath);
		fullpath = NULL;
	}

	return fullpath;
}
