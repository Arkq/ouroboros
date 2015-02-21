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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"


int main(int argc, char **argv) {

	int opt;
	const char *opts = "hc:n:e:i:l:s:p:o:";
	struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"config", required_argument, NULL, 'c'},
		/* runtime configuration */
		{"add-new-nodes", required_argument, NULL, 'n'},
		{"pattern-exclude", required_argument, NULL, 'e'},
		{"pattern-include", required_argument, NULL, 'i'},
		{"kill-latency", required_argument, NULL, 'l'},
		{"kill-signal", required_argument, NULL, 's'},
		{"input-pass-through", required_argument, NULL, 'p'},
		{"output-redirect", required_argument, NULL, 'o'},
		{0, 0, 0, 0},
	};

	struct ouroboros_config config = { 0 };
	char *config_file = NULL;

	/* parse options - first pass */
	while ((opt = getopt_long(argc, argv, opts, longopts, NULL)) != -1)
		switch (opt) {
		case 'h':
return_usage:
			printf("usage: %s [OPTION]... [COMMAND [ARG]...]\n", argv[0]);
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
		case 'e':
			break;
		case 'i':
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

	ouroboros_config_free(&config);
	return EXIT_SUCCESS;
}
