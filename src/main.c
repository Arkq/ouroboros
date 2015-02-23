/*
 * ouroboros - main.c
 * Copyright (c) 2015 Arkadiusz Bokowy
 *
 * This file is a part of an ouroboros.
 *
 * This projected is licensed under the terms of the MIT license.
 *
 */

#if HAVE_CONFIG_H
#include "../config.h"
#endif

#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "debug.h"
#include "notify.h"
#include "process.h"


int main(int argc, char **argv) {

	int opt;
	const char *opts = "hc:n:d:i:e:l:s:p:o:";
	struct option longopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "config", required_argument, NULL, 'c' },
		/* runtime configuration */
		{ OOBSCONF_ADD_NEW_NODES, required_argument, NULL, 'n' },
		{ OOBSCONF_WATCH_DIRECTORY, required_argument, NULL, 'd' },
		{ OOBSCONF_PATTERN_INCLUDE, required_argument, NULL, 'i' },
		{ OOBSCONF_PATTERN_EXCLUDE, required_argument, NULL, 'e' },
		{ OOBSCONF_KILL_LATENCY, required_argument, NULL, 'l' },
		{ OOBSCONF_KILL_SIGNAL, required_argument, NULL, 's' },
		{ OOBSCONF_INPUT_PASS_THROUGH, required_argument, NULL, 'p' },
		{ OOBSCONF_OUTPUT_REDIRECT, required_argument, NULL, 'o' },
		{ 0, 0, 0, 0 },
	};

	struct ouroboros_config config = { 0 };
	char *config_file = NULL;

	/* parse options - first pass */
	while ((opt = getopt_long(argc, argv, opts, longopts, NULL)) != -1)
		switch (opt) {
		case 'h':
return_usage:
			printf("usage: %s [options] <command ...>\n"
					"  -c, --config=FILE\t\tuse this configuration file\n"
					"  -n, --add-new-nodes=BOOL\n"
					"  -d, --watch-directory=DIR\n"
					"  -i, --pattern-include=REGEXP\n"
					"  -e, --pattern-exclude=REGEXP\n"
					"  -l, --kill-latency=VALUE\n"
					"  -s, --kill-signal=VALUE\n"
					"  -p, --input-pass-through=BOOL\n"
					"  -o, --output-redirect=FILE\n",
					argv[0]);
			return EXIT_SUCCESS;

		case 'c':
			config_file = strdup(optarg);
			break;

		case '?':
		case ':':
			fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
			return EXIT_FAILURE;
		}

	if (optind == argc)
		/* we want to run some command, don't we? */
		goto return_usage;

	/* load configuration (internal one and from the file) */
	ouroboros_config_init(&config);
	if (config_file == NULL)
		config_file = get_ouroboros_config_file();
	if (load_ouroboros_config(config_file, argv[optind], &config)) {
		fprintf(stderr, "error: unable to load config file\n");
		return EXIT_FAILURE;
	}

	/* parse options - second pass */
	optind = 0;
	while ((opt = getopt_long(argc, argv, opts, longopts, NULL)) != -1)
		switch (opt) {
		case 'n':
			config.add_new_nodes = ouroboros_config_get_bool(optarg);
			break;
		case 'd':
			ouroboros_config_add_pattern(&config.watch_directory, optarg);
			break;
		case 'i':
			ouroboros_config_add_pattern(&config.pattern_include, optarg);
			break;
		case 'e':
			ouroboros_config_add_pattern(&config.pattern_exclude, optarg);
			break;
		case 'l':
			config.kill_latency = strtol(optarg, NULL, 0);
			break;
		case 's':
			config.kill_signal = ouroboros_config_get_signal(optarg);
			if (!config.kill_signal) {
				fprintf(stderr, "error: unrecognized signal name/value\n");
				return EXIT_FAILURE;
			}
			break;
		case 'p':
			config.input_pass_through = ouroboros_config_get_bool(optarg);
			break;
		case 'o':
			free(config.output_redirect);
			config.output_redirect = strdup(optarg);
			break;
		}

	struct ouroboros_process process = { 0 };
	struct ouroboros_notify notify = { 0 };
	struct pollfd pfds[2];
	char buffer[1024];
	int restart;
	int timeout;
	int rv;

	ouroboros_process_init(&process, argv[optind], &argv[optind]);
	process.output = config.output_redirect;
	process.signal = config.kill_signal;

	ouroboros_notify_init(&notify, config.pattern_include, config.pattern_exclude);
	ouroboros_notify_watch_directories(&notify, config.watch_directory);

	/* poll standard input - IO redirection */
	pfds[0].events = POLLIN;
	pfds[0].fd = config.input_pass_through ? fileno(stdin) : -1;

	/* setup inotify subsystem */
	pfds[1].events = POLLIN;
	pfds[1].fd = notify.fd;

	/* run main maintenance loop */
	for (restart = 1;;) {

		if (restart) {
			restart = 0;
			timeout = -1;
			kill_ouroboros_process(&process);
			if (start_ouroboros_process(&process)) {
				fprintf(stderr, "error: process starting failed\n");
				return EXIT_FAILURE;
			}
		}

		if ((rv = poll(pfds, 2, timeout)) == -1)
			break;  /* signal interruption */

		/* maintain kill latency */
		if (rv == 0) {
			restart = 1;
			continue;
		}

		/* forward received input to the process */
		if (pfds[0].revents & POLLIN) {
			ssize_t rlen = read(pfds[0].fd, buffer, sizeof(buffer));
			if (write(process.stdinfd[1], buffer, rlen) != rlen)
				fprintf(stderr, "warning: data lost during input forwarding\n");
		}

		/* dispatch notification event */
		if (pfds[1].revents & POLLIN) {
			if (ouroboros_notify_dispatch(&notify) == 1)
				timeout = config.kill_latency * 1000;
		}

	}

	/* use signal from the configuration to kill process */
	kill_ouroboros_process(&process);

	/* get the return value of watched process, if possible */
	int retval = EXIT_SUCCESS;
	if (process.status && WIFEXITED(process.status)) {
		retval = WEXITSTATUS(process.status);
		debug("process exit status: %d", retval);
	}

	ouroboros_process_free(&process);
	ouroboros_config_free(&config);
	return retval;
}
