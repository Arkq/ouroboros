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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libconfig.h>


/* Initialize configuration structure with default values. */
void ouroboros_config_init(struct ouroboros_config *config) {
	config->add_new_nodes = 1;
	config->kill_signal = SIGTERM;
}

/* Free allocated resources. */
void ouroboros_config_free(struct ouroboros_config *config) {
	free(config->output_redirect);
}

/* Internal function which actually reads data from the configuration file. */
static void _load_config(const config_setting_t *root, struct ouroboros_config *config) {

	const char *tmp;
	int retval;

	config_setting_lookup_bool(root, OOBSCONF_ADD_NEW_NODES, &config->add_new_nodes);
#define OOBSCONF_PATTERN_EXCLUDE "pattern-exclude"
#define OOBSCONF_PATTERN_INCLUDE "pattern-include"

	config_setting_lookup_int(root, OOBSCONF_KILL_LATENCY, &config->kill_latency);
	if (config_setting_lookup_string(root, OOBSCONF_KILL_SIGNAL, &tmp))
		if ((retval = ouroboros_config_get_signal(tmp)) != 0)
			config->kill_signal = retval;

	config_setting_lookup_bool(root, OOBSCONF_INPUT_PASS_THROUGH, &config->input_pass_through);
	if (config_setting_lookup_string(root, OOBSCONF_OUTPUT_REDIRECT, &tmp))
		config->output_redirect = strdup(tmp);

}

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
		if (!config_setting_lookup_string(extra, OOBSCONF_APP_FILENAME, &tmp))
			continue;
		if (strcmp(tmp, appname) != 0)
			continue;

		/* read extra configuration */
		_load_config(extra, config);

	}

	config_destroy(&cfg);
	return 0;
}

/* Convert given name (string) into the boolean value. */
int ouroboros_config_get_bool(const char *name) {
	if (atoi(name) || strcasecmp(name, "true") == 0)
		return 1;
	return 0;
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
		{ "SIGBUS", SIGBUS },
		{ "SIGPOLL", SIGPOLL },
		{ "SIGPROF", SIGPROF },
		{ "SIGSYS", SIGSYS },
		{ "SIGTRAP", SIGTRAP },
		{ "SIGURG", SIGURG },
		{ "SIGVTALRM", SIGVTALRM },
		{ "SIGXCPU", SIGXCPU },
		{ "SIGXFSZ", SIGXFSZ },
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
