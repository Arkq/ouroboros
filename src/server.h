/*
 * ouroboros - server.h
 * Copyright (c) 2015 Arkadiusz Bokowy
 *
 * This file is a part of a ouroboros.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#ifndef __SERVER_H
#define __SERVER_H

#include <sys/socket.h>


struct ouroboros_server {

	/* interface binding */
	char *ifname;
	struct sockaddr ifaddr;
	int fd;

};


struct ouroboros_server *ouroboros_server_init(const char *ifname, int port);
void ouroboros_server_free(struct ouroboros_server *server);

int ouroboros_server_dispatch(struct ouroboros_server *server);

#endif
