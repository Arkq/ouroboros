/*
 * ouroboros - server.c
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

#include "server.h"

#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "debug.h"


/* Initialize server. This function returns pointer to the initialized server
 * structure or NULL upon error. */
struct ouroboros_server *ouroboros_server_init(const char *ifname, int port) {

	struct ouroboros_server *server;

	if ((server = malloc(sizeof(struct ouroboros_server))) == NULL)
		return NULL;

	server->ifname = ifname != NULL ? strdup(ifname) : NULL;
	server->ifaddr.sa_family = AF_INET;
	((struct sockaddr_in *)(&server->ifaddr))->sin_addr.s_addr = 0;
	server->fd = -1;

	if (ifname == NULL)
		/* server is disabled */
		return server;

	/* get the address of given interface name */
	if (strcmp(ifname, "any") != 0) {
		struct ifaddrs *ifaddr, *ifa;

		if (getifaddrs(&ifaddr) == -1)
			perror("warning: unable to list interfaces");
		else {
			for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
				if (ifa->ifa_addr == NULL || (
							ifa->ifa_addr->sa_family != AF_INET &&
							ifa->ifa_addr->sa_family != AF_INET6))
					continue;
				if (strcmp(ifa->ifa_name, ifname) == 0) {
					memcpy(&server->ifaddr, ifa->ifa_addr, sizeof(server->ifaddr));
					break;
				}
			}

			freeifaddrs(ifaddr);

			if (ifa == NULL) {
				fprintf(stderr, "warning: interface %s not found\n", ifname);
				return server;
			}
		}
	}

	switch (server->ifaddr.sa_family) {
	case AF_INET:
		((struct sockaddr_in *)(&server->ifaddr))->sin_port = htons(port);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)(&server->ifaddr))->sin6_port = htons(port);
		break;
	default:
		fprintf(stderr, "error: unsupported interface: %d\n", server->ifaddr.sa_family);
		ouroboros_server_free(server);
		return NULL;
	}

#if DEBUG
	char tmp[INET6_ADDRSTRLEN];
	debug("binding to: %s:%d", inet_ntop(server->ifaddr.sa_family,
			&((struct sockaddr_in *)(&server->ifaddr))->sin_addr, tmp, sizeof(tmp)), port);
#endif /* DEBUG */

	server->fd = socket(server->ifaddr.sa_family, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (server->fd == -1) {
		perror("error: unable to create socket");
		ouroboros_server_free(server);
		return NULL;
	}

	if (bind(server->fd, &server->ifaddr, sizeof(server->ifaddr)) == -1) {
		perror("error: unable to bind address");
		ouroboros_server_free(server);
		return NULL;
	}

	return server;
}

/* Free allocated resources. */
void ouroboros_server_free(struct ouroboros_server *server) {
	close(server->fd);
	free(server->ifname);
	free(server);
}

/* Dispatch incoming data. If reload was requested, then this function
 * returns 1. Upon error this function returns -1. */
int ouroboros_server_dispatch(struct ouroboros_server *server) {

	char buffer[32];
	ssize_t rlen;

	if ((rlen = read(server->fd, buffer, sizeof(buffer))) == -1)
		perror("warning: unable to read client data");

	debug("message (%zd): %s", rlen, buffer);
	return rlen > 0 ? 1 : 0;
}
