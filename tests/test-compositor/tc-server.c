/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <sys/time.h>
#include <time.h>

#include "wayland-server.h"

#include "tc-utils.h"
#include "tc-server.h"
#include "tc-events.h"
#include "tc-config.h"

/*
 * This configuration is used when no configuration
 * is passed when creating display */
const struct config wit_default_config = {
	CONF_SEAT | CONF_COMPOSITOR,
	CONF_ALL,
	0
};

static void display_create_globals(struct display *d);

/*
 * Terminate display when client exited
 */
static int
handle_sigchld(int signum, void *data)
{
	assertf(signum == SIGCHLD,
		"Got other signal than SIGCHLD from loop\n");
	assertf(data, "Got SIGCHLD with NULL data\n");

	int status, stat;
	struct display *disp = data;

	wl_display_terminate(disp->wl_display);
	dbg("Display terminated\n--\n");

	stat = waitpid(disp->client_pid, &status, WNOHANG);
	assertf(stat != -1, "Waiting for child failed");

	disp->client_exit_code = WEXITSTATUS(status);
	return 0;
}

/* emit n events from d->events eventarray */
static int
emit_events(struct display *d, int n)
{
	int i = 0;
	int m, count;

	assertf(d, "No compositor");
	assertf(n >= 0, "Wrong value of n");
	assertf(d->events, "No eventarray");

	/* how many events can be emitted (for assert()) */
	count = d->events->count - d->events->index;

	if (count == 0) {
		dbg("No events in eventarray\n");
		return 0;
	}

	if (n == 0) { /* 0 means all */
		while(eventarray_emit_one(d,d->events) > 0)
			i++;
		/* i begun at 0, so we have to add 1 before comparing */
		assertf(++i == count, "Emitted %d instead of %d events", i, count);
	} else {
		for (m = 1; i < n && m > 0; i++) {
			m = eventarray_emit_one(d, d->events);
		}
		assertf(i == n || i == count,
			"Emitted %d instead of %d events", i, n);
	}

	return i;
}

/* client called kick_display() to interupt wl_display_run() so that
 * display can progress in code after display_run() */
static int
handle_sigusr1(int signum, void *data)
{
	struct display *disp = data;

	assertf(signum == SIGUSR1,
		"Expected signal %d (SIGUSR1) but got %d", SIGUSR1, signum);
	assertf(data, "Got SIGUSR1 with NULL data\n");

	disp->request = 1;

	/* terminate display, so that we can process request */
	wl_display_terminate(disp->wl_display);

	return 0;
}

void
display_process_request(struct display *disp)
{
	int stat, count, fd;
	enum optype op;
	size_t size;
	struct eventarray *ea;

	assert(disp);
	assertf(disp->request, "We do not have request signalized. "
		"(It can mean that display is not running)");

	wl_display_flush_clients(disp->wl_display);

	fd = disp->client_sock[1];

	/* get orders */
	aread(fd, &op, sizeof(op));

	switch(op) {
		case CAN_CONTINUE:
			assertf(0, "Got CAN_CONTINUE from child");
			break;
		case EVENT_EMIT:
			dbg("Recieving event\n");
			ea = eventarray_recieve(disp);
			assertf(ea->count == 1,
				"Got more than one event");

			dbg("Event recieved .. Emitting\n");
			stat = eventarray_emit_one(disp, ea);
			assertf(stat == 0, "There should be only one event");
			eventarray_free(ea);

			/* acknowledge */
			send_message(fd, EVENT_EMIT, stat);
			break;
		case EVENT_COUNT:
			aread(fd, &count, sizeof(count));

			stat = emit_events(disp, count);
			dbg("Emitted %d events (asked for %d)\n", stat, count);

			/* acknowledge */
			send_message(fd, EVENT_COUNT, stat);
			break;
		case RUN_FUNC:
			dbg("Running user's function\n");
			disp->user_func(disp->user_func_data);

			/* acknowledge */
			send_message(fd, RUN_FUNC);
			break;
		case BARRIER:
			dbg("Syncing display\n");
			send_message(fd, BARRIER);
			break;
		case SEND_BYTES:
			if (disp->data) {
				dbg("SEND_BYTES: Overwritting user data");
				if (disp->data_destroy_func)
					disp->data_destroy_func(disp->data);
			}

			aread(fd, &size, sizeof(size_t));

			disp->data = malloc(size);
			assert(disp->data && "Out of memory");

			aread(fd, disp->data, size);
			disp->data_destroy_func = &free;

			/* acknowledge */
			awrite(fd, &op, sizeof(op));
			awrite(fd, &size, sizeof(size_t));
			break;
		case SEND_EVENTARRAY:
			assertf(0, "Use display_recieve_eventarray() instead");
			break;
		default:
			assertf(0, "Unknown operation");
	}

	disp->request = 0;

	/* continue in wayland's loop */
	wl_display_run(disp->wl_display);
}

/* Since tests can run parallely, we need unique socket names
 * for each test. Otherwise test can fail on wl_display_add_socket.
 * Also test would fail on this function when some other test failed and socket
 * would remain undeleted.
 * Not re-entrant nor thread-safe */
static char *
get_socket_name(void)
{
	struct timeval tv;
	static char retval[108];

	gettimeofday(&tv, NULL);
	snprintf(retval, sizeof retval, "wayland-test-%ld%ld",
		 tv.tv_sec, tv.tv_usec);

	return retval;
}

struct display *
display_create(struct config *conf)
{
	struct display *d = NULL;
	const char *socket_name;
	int stat = 0;

	d = calloc(1, sizeof *d);
	assert(d && "Out of memory");

	if (conf)
		d->config = *conf;
	else
		d->config = wit_default_config;

	d->wl_display = wl_display_create();
	assertf(d->wl_display, "Creating display failed [display: %p]",
		d->wl_display);

	/* hope path won't be longer than 108 .. */
	socket_name = get_socket_name();
	stat = wl_display_add_socket(d->wl_display, socket_name);
	assertf(stat == 0,
		"Failed to add socket '%s' to display. "
		"If everything seems ok, check if path of socket is"
		" shorter than 108 chars or if socket already exists.",
		socket_name);
	dbg("Added socket: %s\n", socket_name);

	d->loop = wl_display_get_event_loop(d->wl_display);
	assertf(d->loop, "Failed to get loop from display");

	d->sigchld = wl_event_loop_add_signal(d->loop, SIGCHLD,
						handle_sigchld, d);
	assertf(d->sigchld,
		"Couldn't add SIGCHLD signal handler to loop");

	d->sigusr1 = wl_event_loop_add_signal(d->loop, SIGUSR1,
						handle_sigusr1, d);
	assertf(d->sigusr1,
		"Couldn't add SIGUSR1 signal handler to loop");

	/* create globals */
	display_create_globals(d);

	stat = socketpair(AF_UNIX, SOCK_STREAM, 0, d->client_sock);
	assertf(stat == 0,
		"Cannot create socket for comunication "
		"between client and server");

	wl_list_init(&d->surfaces);

	return d;
}

inline struct display *
display_create_and_run(struct config *conf,
			   int (*client_main)(int))
{
	struct display *d = display_create(conf);
	display_create_client(d, client_main);
	display_run(d);

	/* so that we can check for errors and free it */
	return d;
}

void
display_destroy(struct display *d)
{
	assert(d && "Invalid pointer given to destroy_compositor");

	struct surface *pos, *tmp;
	int exit_c = d->client_exit_code;

	if (d->data && d->data_destroy_func)
		d->data_destroy_func(d->data);

	if (d->events)
		eventarray_free(d->events);

	close(d->client_sock[0]);
	close(d->client_sock[1]);

	wl_list_for_each_safe(pos, tmp, &d->surfaces, link) {
		free(pos);
	}

	wl_event_source_remove(d->sigchld);
	wl_event_source_remove(d->sigusr1);

	wl_display_destroy(d->wl_display);

	free(d);

	assertf(exit_c == EXIT_SUCCESS, "Client exited with %d", exit_c);
}

void
display_run(struct display *d)
{
	assert(d && "Wrong pointer");

	/* Client waits until display initialize itself.
	 * Let client know that he can stop waiting and continue */
	send_message(d->client_sock[1], CAN_CONTINUE, 1);

	wl_display_run(d->wl_display);
}

static int
run_client(int (*client_main)(int), int wayland_sock, int client_sock)
{
	char s[32];
	enum optype op = 0;
	int can_continue = 0;

	/* Wait until display signals that client can continue */
	aread(client_sock, &op, sizeof(op));
	aread(client_sock, &can_continue, sizeof(int));

	assertf(op == CAN_CONTINUE,
		"Got request for another operation (%u) than CAN_CONTINUE (%d)",
		op, CAN_CONTINUE);
	assertf(can_continue == 0 || can_continue == 1,
		"CAN_CONTINUE can be either 0 or 1");

	if (can_continue == 0)
		return EXIT_FAILURE;

	/* for wl_display_connect() */
	snprintf(s, sizeof s, "%d", wayland_sock);
	setenv("WAYLAND_SOCKET", s, 0);

	return client_main(client_sock);
}

static inline void
handle_child_abort(int signum)
{
	assertf(signum == SIGABRT,
		"Got another signal than SIGABRT");

	_exit(SIGABRT);
}

void
display_create_client(struct display *disp,
			  int (*client_main)(int))
{
	int sockv[2];
	pid_t pid;
	int stat;
	int test = 0;

	stat = socketpair(AF_UNIX, SOCK_STREAM, 0, sockv);
	assertf(stat == 0,
		"Failed to create socket pair");

	pid = fork();
	assertf(pid != -1, "Fork failed");

	if (pid == 0) {
		close(sockv[1]);
		close(disp->client_sock[1]);

		/* just test if connection is established */
		aread(disp->client_sock[0], &test, sizeof(int));
		assertf(test == 0xbeef, "Connection error");
		test = 0xdaf;
		awrite(disp->client_sock[0], &test, sizeof(int));

		/* abort() itself doesn't imply failing test when it's forked,
		 * we need call exit after abort() */
		signal(SIGABRT, handle_child_abort);
		stat = run_client(client_main, sockv[0],
				  disp->client_sock[0]);

		close(disp->client_sock[0]);
		close(sockv[0]);

		exit(stat);
	} else {
		close(sockv[0]);
		close(disp->client_sock[0]);

		disp->client_pid = pid;

		/* just test if connection is established */
		test = 0xbeef;
		awrite(disp->client_sock[1], &test, sizeof(int));
		aread(disp->client_sock[1], &test, sizeof(int));
		assertf(test == 0xdaf, "Connection error");

		disp->wl_client = wl_client_create(disp->wl_display, sockv[1]);
		if (!disp->wl_client) {
			send_message(disp->client_sock[1], CAN_CONTINUE, 0);
			assertf(disp->wl_client, "Couldn't create wayland client");
		}
	}
}

/**
 * Set user's data and it's destructor
 * (destructor will be called in display_destroy)
 */
void
display_add_user_data(struct display *disp, void *data,
			  void (*destroy_func)(void *))
{
	assert(disp);

	ifdbg(disp->data, "Overwriting user data\n");

	disp->data = data;
	disp->data_destroy_func = destroy_func;
}

inline void *
display_get_user_data(struct display *disp)
{
	return disp->data;
}

inline void
display_add_user_func(struct display *disp,
			  void (*func) (void *), void *data)
{
	disp->user_func = func;
	disp->user_func_data = data;
}

void
display_add_events(struct display *d, struct eventarray *e)
{
	assert(d);
	ifdbg(d->events, "Rewriting old eventarray\n");

	d->events = e;
}

void
display_recieve_eventarray(struct display *d)
{
	dbg("Recieving eventarray\n");

	ifdbg(d->events, "Overwriting events\n");

	enum optype op = SEND_EVENTARRAY;
	struct eventarray *ea = eventarray_recieve(d);
	dbg("Eventarray recieved\n");

	/* acknowledge */
	awrite(d->client_sock[1], &op, sizeof(op));
	awrite(d->client_sock[1], &ea->count, sizeof(unsigned));

	d->events = ea;

	/* continue working */
	wl_display_run(d->wl_display);
}

/*
 * Wayland bindings
 */

/* definitions can be found in server-protocol.c */
void seat_bind(struct wl_client *, void *, uint32_t, uint32_t);
void compositor_bind(struct wl_client *, void *, uint32_t, uint32_t);

/* create globals in display according to configuration */
static void
display_create_globals(struct display *d)
{
	assert(d);

	if (d->config.globals == 0)
		return;

	if (d->config.globals & CONF_SEAT) {
		d->globals.wl_seat =
			wl_global_create(d->wl_display, &wl_seat_interface,
					 wl_seat_interface.version,
					 d, seat_bind);
		assertf(d->globals.wl_seat, "Failed creating global for seat");
	}

	if (d->config.globals & CONF_COMPOSITOR) {
		d->globals.wl_compositor =
			wl_global_create(d->wl_display, &wl_compositor_interface,
					 wl_compositor_interface.version,
					 d, compositor_bind);
		assertf(d->globals.wl_compositor,
			"Failed creating global for compositor");
	}

	if (d->config.globals & CONF_SHM) {
		assertf(wl_display_init_shm(d->wl_display) == 0,
			"Failed shm init");
	}
}
