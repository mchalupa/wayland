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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "wayland-client.h"

#include "tc-utils.h"
#include "tc-client.h"
#include "tc-events.h"

void
client_init(struct client *c, int s)
{
	assert(c);
	assert(s >= 0);

	memset(c, 0, sizeof *c);

	c->sock = s;
	c->display = wl_display_connect(NULL);
	assertf(c->display, "Couldn't connect to display");
}

inline void
client_deinit(struct client *c)
{
	wl_display_disconnect(c->display);
}

/*
 * Allow comfortably add listener into client structure
 * XXX Do it somehow else.. or let user manually add listeners
 */
void
client_add_listener(struct client *cl, const char *interface,
			const void *listener)
{
	assertf(cl);
	assert(interface);

	ifdbg(listener == NULL, "Adding NULL listener (%s)\n", interface);

	if (strcmp(interface, "wl_pointer") == 0) {
		ifdbg(cl->pointer.listener, "Rewriting pointer listener (%p)\n",
		      cl->pointer.listener);
		cl->pointer.listener = listener;

		if (cl->pointer.proxy)
			wl_pointer_add_listener(
				(struct wl_pointer *) cl->pointer.proxy,
				(struct wl_pointer_listener *) cl->pointer.listener,
				cl);
		else
			dbg("Not adding listener."
                            "Pointer proxy hasn't been created yet.\n");
	} else if (strcmp(interface, "wl_keyboard") == 0) {
		ifdbg(cl->keyboard.listener, "Rewriting keyboard listener (%p)\n",
		      cl->keyboard.listener);
		cl->keyboard.listener = listener;

		if (cl->keyboard.proxy)
			wl_keyboard_add_listener(
				(struct wl_keyboard *) cl->keyboard.proxy,
				(struct wl_keyboard_listener *) cl->keyboard.listener,
				cl);
		else
			dbg("Not adding listener."
                            "Keyboard proxy hasn't been created yet.\n");
	} else if (strcmp(interface, "wl_touch") == 0) {
		ifdbg(cl->touch.listener, "Rewriting touch listener (%p)\n",
		      cl->touch.listener);
		cl->touch.listener = listener;

		if (cl->touch.proxy)
			wl_touch_add_listener(
				(struct wl_touch *) cl->touch.proxy,
				(struct wl_touch_listener *) cl->touch.listener,
				cl);
		else
			dbg("Not adding listener."
                            "Touch proxy hasn't been created yet.\n");
	} else if (strcmp(interface, "wl_seat") == 0) {
		ifdbg(cl->seat.listener, "Rewriting seat listener (%p)\n",
		      cl->seat.listener);
		cl->seat.listener = listener;

		if (cl->seat.proxy)
			wl_seat_add_listener(
				(struct wl_seat *) cl->seat.proxy,
				(struct wl_seat_listener *) cl->seat.listener,
				cl);
		else
			dbg("Not adding listener."
                            "seat proxy hasn't been created yet.\n");
	} else if (strcmp(interface, "wl_shm") == 0) {
		ifdbg(cl->shm.listener, "Rewriting shm listener (%p)\n",
		      cl->shm.listener);
		cl->shm.listener = listener;

		if (cl->shm.proxy)
			wl_shm_add_listener(
				(struct wl_shm *) cl->shm.proxy,
				(struct wl_shm_listener *) cl->shm.listener,
				cl);
		else
			dbg("Not adding listener."
                            "shm proxy hasn't been created yet.\n");
	} else if (strcmp(interface, "wl_registry") == 0) {
		ifdbg(cl->registry.listener, "Rewriting registry listener (%p)\n",
		      cl->registry.listener);
		cl->registry.listener = listener;

		if (cl->registry.proxy)
			wl_registry_add_listener(
				(struct wl_registry *) cl->registry.proxy,
				(struct wl_registry_listener *) cl->registry.listener,
				cl);
		else
			dbg("Not adding listener."
                            "registry proxy hasn't been created yet.\n");
	} else {
		assertf(0, "Unknown type of interface");
	}
}

struct client *
client_populate(int sock)
{
	struct client *c = calloc(1, sizeof *c);
	assert(c && "Out of memory");

	c->sock = sock;

	c->display = wl_display_connect(NULL);
	assertf(c->display, "Couldn't connect to display");

	c->registry.proxy =
		(struct wl_proxy *) wl_display_get_registry(c->display);
	assertf(c->registry.proxy, "Couldn't get registry");

	client_add_listener(c, "wl_registry", &registry_default_listener);
	wl_display_dispatch(c->display);

	assertf(wl_display_get_error(c->display) == 0,
		"An error in display occured");

	return c;
}

static void
client_object_destroy(struct client_object *obj,
			void (*proxy_dest_func)(struct wl_proxy *))
{
	if (! obj)
		return;

	if (proxy_dest_func == NULL)
		proxy_dest_func = wl_proxy_destroy;

	/* destory data if we have been given destructor */
	if (obj->data && obj->data_destr)
		obj->data_destr(obj->data);

	if (obj->proxy)
		/* compiler screams, so retype it .. */
		proxy_dest_func((void *) obj->proxy);

	/* suppose we're destroying object in client's struct, so this
	 * will have effect */
	memset(obj, 0, sizeof(struct client_object));
}

void
client_free(struct client *c)
{
	assertf(c, "Wrong pointer");

	/* do everything what left */
	wl_display_roundtrip(c->display);
	assertf(wl_display_get_error(c->display) == 0,
		"An error in display occured");

	client_object_destroy(&c->compositor, (void *) &wl_compositor_destroy);
	client_object_destroy(&c->seat, (void *) &wl_seat_destroy);
	client_object_destroy(&c->pointer, (void *) &wl_pointer_destroy);
	client_object_destroy(&c->keyboard, (void *) &wl_keyboard_destroy);
	client_object_destroy(&c->touch, (void *) &wl_touch_destroy);
	client_object_destroy(&c->registry, (void *) &wl_registry_destroy);

	wl_display_disconnect(c->display);
	close(c->sock);

	free(c);
}

static void
kick_display(struct client *c)
{
	assert(c);

	wl_display_flush(c->display);
	wl_display_dispatch_pending(c->display);

	int stat = kill(getppid(), SIGUSR1);
	assertf(stat == 0,
		"Failed sending SIGUSR1 signal to display");
}

static inline void
get_acknowledge(int fd, enum optype op)
{
	enum optype acknop;

	aread(fd, &acknop, sizeof(op));
	assertf(op == acknop, "Got bad acknowledge (%d instead of %d)", op,
		acknop);
}

void
client_call_user_func(struct client *cl)
{
	dbg("Request for user func\n");

	kick_display(cl);
	send_message(cl->sock, RUN_FUNC);
	get_acknowledge(cl->sock, RUN_FUNC);
}

void
client_send_eventarray(struct client *cl, struct eventarray *ea)
{
	unsigned count;

	dbg("Sending eventarray to display\n");

	kick_display(cl);
	eventarray_send(cl, ea);

	get_acknowledge(cl->sock, SEND_EVENTARRAY);

	aread(cl->sock, &count, sizeof(unsigned));
	assertf(count == ea->count,
                "Display replied that it got different number of events"
		" (%u and %u)", count, ea->count);
}

void
client_trigger_event(struct client *cl, const struct event *e, ...)
{
	va_list vl;
	struct eventarray *ea;

	dbg("Sending event to display\n");

	va_start(vl, e);
	ea = eventarray_create();
	eventarray_add_vl(ea, CLIENT, e, vl);
	va_end(vl);

	kick_display(cl);
	send_message(cl->sock, EVENT_EMIT);
	eventarray_send(cl, ea);
	get_acknowledge(cl->sock, EVENT_EMIT);

	eventarray_free(ea);
}

void
client_send_data(struct client *cl, void *src, size_t size)
{
	size_t got_size;

	dbg("Sending data to display\n");

	kick_display(cl);
	send_message(cl->sock, SEND_BYTES, src, size);
	get_acknowledge(cl->sock, SEND_BYTES);

	aread(cl->sock, &got_size, sizeof(size_t));
	assertf(got_size == size,
                "Display replied that it got different number of bytes"
		" (%lu and %lu)", size, got_size);
}

void
client_recieve_data(struct client *cl, void **src, size_t *size)
{
	assert(cl && src);
	dbg("Recieving data from display\n");

	size_t count;
	enum optype op;

	/* The display is in wl_display_run() cycle. Kick it to
	 * break from the cycle and let it send the data to client */
	kick_display(cl);

	/* send_message is used, so the first data is optype */
	aread(cl->sock, &op, sizeof(enum optype));
	assertf(op == SEND_BYTES,
		"Wrong operation, expected SEND_BYTES but got [%d]", op);

	aread(cl->sock, &count, sizeof(size_t));
	if (size)
		*size = count;

	*src = malloc(count);
	assert(src && "Out of memory");

	aread(cl->sock, *src, count);

	/* acknowledge */
	awrite(cl->sock, &count, sizeof(size_t));
}

int
client_ask_for_events(struct client *cl, int n)
{
	int count;

	dbg("Request for events(%p, %d)\n", cl, n);

	kick_display(cl);
	send_message(cl->sock, EVENT_COUNT, n);
	get_acknowledge(cl->sock, EVENT_COUNT);

	aread(cl->sock, &count, sizeof(count));

	cl->emitting = 1;

	return count;
}

void
client_barrier(struct client *cl)
{
	kick_display(cl);
	send_message(cl->sock, BARRIER);
	get_acknowledge(cl->sock, BARRIER);

	dbg("Barrier: client synced\n");
}

void
client_state(struct client *cl)
{
	assert(cl);

	dbg("Client current state [%p]:\n"
	    "        Display: %s\n"
	    "        Emitting: %s\n"
	    "        Proxies: %s %s %s %s %s\n"
	    "        Listeners: %s %s %s %s %s\n",
	    cl,
	    cl->display ? "yes" : "no",
	    cl->emitting ? "yes" : "no",
	    cl->registry.proxy ? "registry" : "*",
	    cl->seat.proxy ? "seat" : "*",
	    cl->pointer.proxy ? "pointer" : "*",
	    cl->keyboard.proxy ? "keyboard" : "*",
	    cl->touch.proxy ? "touch" : "*",
	    cl->registry.listener ? "registry" : "*",
	    cl->seat.listener  ? "seat" : "*",
	    cl->pointer.listener  ? "pointer" : "*",
	    cl->keyboard.listener  ? "keyboard" : "*",
	    cl->touch.listener  ? "touch" : "*");
}
