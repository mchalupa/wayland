noinst_LTLIBRARIES = libtest-runner.la libtest-helpers.la

libtest_runner_la_LDFLAGS = -ldl
libtest_runner_la_LIBADD = libtest-helpers.la
libtest_runner_la_SOURCES =	\
	test-runner.c		\
	test-runner.h

libtest_helpers_la_SOURCES =	\
	test-helpers.c

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src
AM_CFLAGS = $(GCC_CFLAGS)
