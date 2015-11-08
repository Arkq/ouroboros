# ouroboros - Makefile.am
# Copyright (c) 2015 Arkadiusz Bokowy

TESTS = \
	test-config \
	test-ouroboros.sh

check_PROGRAMS = \
	test-config

test_config_SOURCES = test-config.c ../src/config.c
test_config_CFLAGS = -I../src @LIBCONFIG_CFLAGS@
test_config_LDADD = @LIBCONFIG_LIBS@
