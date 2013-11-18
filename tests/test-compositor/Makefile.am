noinst_LTLIBRARIES =	\
	libtc-utils.la	\
	libtc-server.la	\
	libtc-client.la

wayland_src = $(top_srcdir)/src

libtc_utils_la_LIBADD =
libtc_utils_la_SOURCES = tc-utils.c tc-config.c

libtc_server_la_LIBADD =
	libtc-utils.la				\
	$(wayland_src)/libwayland-server.la
libtc_server_la_SOURCES = 	\
	tc-server.c		\
	server-protocol.c	\
	tc-events.c

libtc_client_la_LIBADD =				\
	libtc-utils.la					\
	$(wayland_src)/libwayland-client.la
libtc_client_la_SOURCES = tc-client.c client-protocol.c

AM_CPPFLAGS = -I$(top_srcdir)/src
