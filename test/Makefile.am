# ouroboros - Makefile.am
# Copyright (c) 2015 Arkadiusz Bokowy

TESTS = \
	test-config \
	test-ouroboros.sh

check_PROGRAMS = \
	test-config

test_config_SOURCES = test-config.c $(top_srcdir)/src/config.c
test_config_CFLAGS = -I$(top_srcdir)/src @LIBCONFIG_CFLAGS@
test_config_LDADD = @LIBCONFIG_LIBS@
