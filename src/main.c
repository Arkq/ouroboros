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

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "config.h"
#include "debug.h"
#include "notify.h"
#include "process.h"
#if ENABLE_SERVER
#include "server.h"
#endif


/* Global variable and handler (callback) for signal redirections. */
static pid_t sr_pid = 0;
static void sr_handler(int sig) {
	debug("redirecting signal: %d", sig);
	if (sr_pid)
		kill(sr_pid, sig);
}

/* Initialize signal redirections. Note, that not all signals can be
 * caught and therefore redirected. This restriction is beyond our
 * power, so we will simply show appropriate warning in such cases. */
static void setup_signals(int *signals) {

	if (signals == NULL)
		return;

	struct sigaction sigact = { 0 };
	sigact.sa_handler = sr_handler;

	while (*signals) {
		if (sigaction(*signals, &sigact, NULL) == -1)
			perror("warning: unable to install signal handler");
		signals++;
	}

}

int main(int argc, char **argv) {

	int opt;
	const char *opts = "hc:E:p:r:u:i:e:k:l:a:t:o:s:";
	struct option longopts[] = {
		{ "help", no_argument, NULL, 'h' },
#if ENABLE_LIBCONFIG
		{ "config", required_argument, NULL, 'c' },
#endif
		/* runtime configuration */
		{ OCKD_WATCH_ENGINE, required_argument, NULL, 'E' },
		{ OCKD_WATCH_PATH, required_argument, NULL, 'p' },
		{ OCKD_WATCH_RECURSIVE, required_argument, NULL, 'r' },
		{ OCKD_WATCH_UPDATE_NODES, required_argument, NULL, 'u' },
		{ OCKD_WATCH_INCLUDE, required_argument, NULL, 'i' },
		{ OCKD_WATCH_EXCLUDE, required_argument, NULL, 'e' },
		{ OCKD_KILL_SIGNAL, required_argument, NULL, 'k' },
		{ OCKD_KILL_LATENCY, required_argument, NULL, 'l' },
		{ OCKD_START_LATENCY, required_argument, NULL, 'a' },
		{ OCKD_REDIRECT_INPUT, required_argument, NULL, 't' },
		{ OCKD_REDIRECT_OUTPUT, required_argument, NULL, 'o' },
		{ OCKD_REDIRECT_SIGNAL, required_argument, NULL, 's' },
		{ 0, 0, 0, 0 },
	};

	struct ouroboros_config config = { 0 };
#if ENABLE_LIBCONFIG
	char *config_file = NULL;
#endif

	/* parse options - first pass */
	while ((opt = getopt_long(argc, argv, opts, longopts, NULL)) != -1)
		switch (opt) {
		case 'h':
return_usage:
			printf("usage: %s [options] [--] <command ...>\n"
#if ENABLE_LIBCONFIG
					"  -c, --config=FILE\t\tuse this configuration file\n"
#endif
					"  -E, --watch-engine=TYPE\n"
					"  -p, --watch-path=DIR\n"
					"  -r, --watch-recursive=BOOL\n"
					"  -u, --watch-update-nodes=BOOL\n"
					"  -i, --watch-include=REGEXP\n"
					"  -e, --watch-exclude=REGEXP\n"
					"  -k, --kill-signal=SIG\n"
					"  -l, --kill-latency=VALUE\n"
					"  -a, --start-latency=VALUE\n"
					"  -t, --redirect-input=BOOL\n"
					"  -o, --redirect-output=FILE\n"
					"  -s, --redirect-signal=SIG\n",
					argv[0]);
			return EXIT_SUCCESS;

#if ENABLE_LIBCONFIG
		case 'c':
			config_file = strdup(optarg);
			break;
#endif /* ENABLE_LIBCONFIG */

		case '?':
		case ':':
			fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
			return EXIT_FAILURE;
		}

	if (optind == argc)
		/* we want to run some command, don't we? */
		goto return_usage;

	/* initialize default configuration */
	ouroboros_config_init(&config);

#if ENABLE_LIBCONFIG
	/* load configuration from the given file or default one */
	if (config_file == NULL)
		config_file = get_ouroboros_config_file();
	if (load_ouroboros_config(config_file, argv[optind], &config))
		fprintf(stderr, "warning: unable to load configuration file\n");
	free(config_file);
#endif /* ENABLE_LIBCONFIG */

	/* parse options - second pass */
	optind = 0;
	while ((opt = getopt_long(argc, argv, opts, longopts, NULL)) != -1)
		switch (opt) {
		case 'E':
			if ((opt = ouroboros_config_get_engine(optarg)) == -1)
				fprintf(stderr, "warning: unrecognized engine: %s\n", optarg);
			else
				config.engine = opt;
			break;
		case 'p':
			ouroboros_config_add_string(&config.watch_paths, optarg);
			break;
		case 'r':
			config.watch_recursive = ouroboros_config_get_bool(optarg);
			break;
		case 'u':
			config.watch_update_nodes = ouroboros_config_get_bool(optarg);
			break;
		case 'i':
			ouroboros_config_add_string(&config.watch_includes, optarg);
			break;
		case 'e':
			ouroboros_config_add_string(&config.watch_excludes, optarg);
			break;
		case 'k':
			if ((opt = ouroboros_config_get_signal(optarg)) == 0)
				fprintf(stderr, "warning: unrecognized signal: %s\n", optarg);
			else
				config.kill_signal = opt;
			break;
		case 'l':
			config.kill_latency = strtod(optarg, NULL);
			break;
		case 'a':
			config.start_latency = strtod(optarg, NULL);
			break;
		case 't':
			config.redirect_input = ouroboros_config_get_bool(optarg);
			break;
		case 'o':
			free(config.redirect_output);
			config.redirect_output = strdup(optarg);
			break;
		case 's':
			if ((opt = ouroboros_config_get_signal(optarg)) == 0)
				fprintf(stderr, "warning: unrecognized signal: %s\n", optarg);
			else
				ouroboros_config_add_int(&config.redirect_signals, opt);
			break;
		}

	enum action {
		ACTION_NONE = 0,
		ACTION_KILL,
		ACTION_START,
	};

	struct ouroboros_process process;
	struct ouroboros_notify *notify;
	struct ouroboros_server *server;
	struct pollfd pfds[3];
	char buffer[1024];
	enum action action;
	int timeout;
	int rv;

	/* it is our crucial subsystem - running without it is pointless */
	if ((notify = ouroboros_notify_init(config.engine)) == NULL)
		return EXIT_FAILURE;

	/* non-recursive mode implicitly excludes updates */
	if (!config.watch_recursive)
		config.watch_update_nodes = 0;

	/* since this tool is intended to be be used for development, it might be
	 * a nice addition to disable the buffering on the standard output */
	setvbuf(stdout, NULL, _IONBF, 0);

	ouroboros_notify_recursive(notify, config.watch_recursive);
	ouroboros_notify_update_nodes(notify, config.watch_update_nodes);
	ouroboros_notify_dirs_only(notify, config.watch_dirs_only);
	ouroboros_notify_files_only(notify, config.watch_files_only);
	ouroboros_notify_include_patterns(notify, config.watch_includes);
	ouroboros_notify_exclude_patterns(notify, config.watch_excludes);
	ouroboros_notify_watch(notify, config.watch_paths);

#if ENABLE_SERVER
	server = ouroboros_server_init(config.server_iface, config.server_port);
	if (server == NULL)
		return EXIT_FAILURE;
#endif /* ENABLE_SERVER */

	ouroboros_process_init(&process, argv[optind], &argv[optind]);
	process.output = config.redirect_output;
	process.signal = config.kill_signal;

	/* set up signal redirections */
	setup_signals(config.redirect_signals);

	/* poll standard input - IO redirection */
	pfds[0].events = POLLIN;
	pfds[0].fd = config.redirect_input ? fileno(stdin) : -1;

	/* setup inotify subsystem */
	pfds[1].events = POLLIN;
	pfds[1].fd = -1;
#if HAVE_SYS_INOTIFY_H
	if (config.engine == ONT_INOTIFY)
		pfds[1].fd = notify->s.inotify.fd;
#endif /* HAVE_SYS_INOTIFY_H */

	/* setup server subsystem */
	pfds[2].events = POLLIN;
	pfds[2].fd = -1;
#if ENABLE_SERVER
	pfds[2].fd = server->fd;
#endif

	/* run main maintenance loop */
	for (action = ACTION_START;;) {

		if (timeout == -1)
			switch (action) {

			case ACTION_KILL:
				action = ACTION_START;
				timeout = config.start_latency * 1000;

				kill_ouroboros_process(&process);

				break;

			case ACTION_START:
				action = ACTION_NONE;
				timeout = -1;

				if (start_ouroboros_process(&process)) {
					fprintf(stderr, "error: process starting failed\n");
					return EXIT_FAILURE;
				}

				/* update pid for signal redirection */
				sr_pid = process.pid;

			}

		/* update interval for poll notification type */
		if (config.engine == ONT_POLL && timeout == -1)
			/* TODO: dedicated value for polling interval */
			timeout = config.kill_latency * 1000;

		debug("poll timeout: %d", timeout);
		if ((rv = poll(pfds, 3, timeout)) == -1) {
			if (errno == EINTR)
				/* signal interruption, not a big deal */
				continue;
			perror("error: poll failed");
			break;
		}

		/* timeout handling */
		if (rv == 0) {
			timeout = -1;
			/* maintain intervals for poll notification type */
			if (config.engine == ONT_POLL && action != ACTION_START) {
				if (ouroboros_notify_dispatch(notify)) {
					action = ACTION_KILL;
					timeout = config.kill_latency * 1000;
				}
			}
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
			if (ouroboros_notify_dispatch(notify) == 1) {
				action = ACTION_KILL;
				timeout = config.kill_latency * 1000;
			}
		}

#if ENABLE_SERVER
		/* dispatch server incoming data */
		if (pfds[2].revents & POLLIN) {
			if (ouroboros_server_dispatch(server) == 1) {
				action = ACTION_KILL;
				timeout = -1;
			}
		}
#endif /* ENABLE_SERVER */

	}

	/* use signal from the configuration to kill process */
	kill_ouroboros_process(&process);

	/* get the return value of watched process, if possible */
	rv = EXIT_SUCCESS;
	if (process.status && WIFEXITED(process.status)) {
		rv = WEXITSTATUS(process.status);
		debug("process exit status: %d", rv);
	}

	ouroboros_process_free(&process);
#if ENABLE_SERVER
	ouroboros_server_free(server);
#endif
	ouroboros_notify_free(notify);
	ouroboros_config_free(&config);
	return rv;
}
