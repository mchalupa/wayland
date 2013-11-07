SUBDIRS = test-runner test-compositor

test_runner_dir = $(top_builddir)/tests/test-runner
test_compositor_dir = $(top_builddir)/tests/test-compositor

test_compositor_ldadd =				\
	$(test_compositor_dir)/libtc-server.la	\
	$(test_compositor_dir)/libtc-client.la

TESTS =						\
	array-test				\
	client-test				\
	display-test				\
	connection-test				\
	event-loop-test				\
	fixed-test				\
	list-test				\
	map-test				\
	os-wrappers-test			\
	sanity-test				\
	socket-test				\
	queue-test				\
	signal-test				\
	resources-test				\
	wl_shm-test				\
	wl_proxy-test				\
	wl_global-test				\
	test-compositor-test			\
	tc-events-test

check_PROGRAMS =				\
	$(TESTS)				\
	exec-fd-leak-checker

noinst_PROGRAMS =				\
	fixed-benchmark

array_test_SOURCES = array-test.c
client_test_SOURCES = client-test.c
display_test_SOURCES = display-test.c
connection_test_SOURCES = connection-test.c
event_loop_test_SOURCES = event-loop-test.c
fixed_test_SOURCES = fixed-test.c
list_test_SOURCES = list-test.c
map_test_SOURCES = map-test.c
sanity_test_SOURCES = sanity-test.c
socket_test_SOURCES = socket-test.c
queue_test_SOURCES = queue-test.c
signal_test_SOURCES = signal-test.c
resources_test_SOURCES = resources-test.c
wl_shm_test_SOURCES = wl_shm-test.c
wl_proxy_test_SOURCES = wl_proxy-test.c
wl_global_test_SOURCES = wl_global-test.c
test_compositor_test_SOURCES = test-compositor-test.c
tc_events_test_SOURCES = tc-events-test.c

fixed_benchmark_SOURCES = fixed-benchmark.c

os_wrappers_test_SOURCES = 			\
	os-wrappers-test.c			\
	../src/wayland-os.c

AM_CPPFLAGS =					\
	-I$(top_builddir)/src			\
	-I$(top_srcdir)/src			\
	-I$(test_runner_dir)			\
	-I$(test_compositor_dir)
AM_CFLAGS = $(GCC_CFLAGS) $(FFI_CFLAGS)
LDADD = $(top_builddir)/src/libwayland-util.la		\
	$(top_builddir)/src/libwayland-client.la 	\
	$(top_builddir)/src/libwayland-server.la 	\
	$(test_runner_dir)/libtest-runner.la		\
	$(test_compositor_ldadd)
	-lrt -ldl $(FFI_LIBS)

exec_fd_leak_checker_SOURCES =			\
	exec-fd-leak-checker.c
exec_fd_leak_checker_LDADD = $(test_runner_dir)/libtest-helpers.la
