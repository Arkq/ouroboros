# ouroboros - Makefile.am
# Copyright (c) 2015 Arkadiusz Bokowy

bin_PROGRAMS = ouroboros

ouroboros_SOURCES = \
	config.c \
	notify.c \
	process.c \
	main.c

ouroboros_CFLAGS = \
	@LIBCONFIG_CFLAGS@ \
	@LIBPROCPS_CFLAGS@

ouroboros_LDADD = \
	@LIBCONFIG_LIBS@ \
	@LIBPROCPS_LIBS@

if ENABLE_SERVER
ouroboros_SOURCES += server.c
endif
