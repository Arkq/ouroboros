# ouroboros - configure.ac
# Copyright (c) 2015 Arkadiusz Bokowy

AC_INIT([ouroboros], [0.0.1], [arkadiusz.bokowy@gmail.com])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AC_PROG_CC
AM_PROG_CC_C_O


# support for debugging
AC_ARG_ENABLE(
	[debug],
	[AS_HELP_STRING([--enable-debug], [enable debugging support])],
)
AM_CONDITIONAL([ENABLE_DEBUG], [test "x$enable_debug" = "xyes"])
AM_COND_IF(
	[ENABLE_DEBUG],
	[AC_DEFINE([DEBUG], [1], [Define to 1 if the debugging is enabled])],
)


# notification subsystem support for Linux
AC_CHECK_HEADERS([sys/inotify.h])

# processes management
PKG_CHECK_MODULES(
	[LIBPROCPS], [libprocps],
	[], [AC_MSG_ERROR([module libproc not found])],
)


# support for the libconfig
AC_ARG_ENABLE(
	[libconfig],
	[AS_HELP_STRING([--enable-libconfig], [enable libconfig support])],
)
AM_CONDITIONAL([ENABLE_LIBCONFIG], [test "x$enable_libconfig" = "xyes"])
AM_COND_IF(
	[ENABLE_LIBCONFIG],
	[PKG_CHECK_MODULES(
		[LIBCONFIG], [libconfig],
		[AC_DEFINE([ENABLE_LIBCONFIG], [1], [Define to 1 if libconfig is enabled])],
		[AC_MSG_ERROR([module libconfig not found])],
	)],
)

# support for the iniparser
AC_ARG_ENABLE(
	[iniparser],
	[AS_HELP_STRING([--enable-iniparser], [enable iniparser support])],
)
AM_CONDITIONAL([ENABLE_INIPARSER], [test "x$enable_iniparser" = "xyes"])
AM_COND_IF(
	[ENABLE_INIPARSER],
	[
		AM_COND_IF(
			[ENABLE_LIBCONFIG],
			[], [AC_MSG_ERROR([iniparser requires libconfig to be enabled])],
		)
		AC_CHECK_HEADERS(
			[iniparser.h],
			[], [AC_MSG_ERROR([iniparser.h header not found])],
		)
		AC_CHECK_LIB(
			[iniparser], [iniparser_load],
			[], [AC_MSG_ERROR([iniparser library not found])],
		)
		AC_DEFINE([ENABLE_INIPARSER], [1], [Define to 1 if iniparser is enabled])
	],
)

# support for server/client interaction
AC_ARG_ENABLE(
	[server],
	[AS_HELP_STRING([--enable-server], [enable server support])],
)
AM_CONDITIONAL([ENABLE_SERVER], [test "x$enable_server" = "xyes"])
AM_COND_IF(
	[ENABLE_SERVER],
	[AC_DEFINE([ENABLE_SERVER], [1], [Define to 1 if server is enabled])],
)


AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_OUTPUT
