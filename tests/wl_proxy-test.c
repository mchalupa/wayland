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

#include <stdio.h>
#include <string.h>

#include "wayland-server.h"
#include "wayland-client.h"

#include "test-runner.h"
#include "test-compositor.h"

/* ---------------------------------
 *  Dummy object
 * ------------------------------- */
struct wl_dummy;

const struct wl_interface dummy_interface;
const struct wl_interface *types[] = {
	NULL,
	&dummy_interface,
};

static const struct wl_message dummy_requests[] = {
	{ "request_empty", "",  types + 0},
	{ "request_i", "i",  types + 0},
	{ "request_s", "s",  types + 1},
	/* TODO fill in other types */
};

static const struct wl_message dummy_events[] = {
	{ "event_empty", "", types + 0},
	{ "event_i", "i", types + 0},
	{ "event_s", "s", types + 1},
};

#define EVENTS_NO 3
#define REQUESTS_NO 3
const struct wl_interface wl_dummy_interface = {
	"wl_dummy", 1,
	EVENTS_NO, dummy_requests,
	REQUESTS_NO, dummy_events,
};

struct wl_dummy_interface {
	void (*request_empty) (struct wl_client *client, struct wl_resource *resource);
	void (*request_i) (struct wl_client *client, struct wl_resource *resource, int i);
	void (*request_s) (struct wl_client *client, struct wl_resource *resource,
			   const char *s);
};

struct wl_dummy_listener {
	void (*event_empty) (void *data, struct wl_dummy *wl_dummy);
	void (*event_i) (void *data, struct wl_dummy *wl_dummy, int i);
	void (*event_s) (void *data, struct wl_dummy *wl_dummy,
			 const char *s);
};

/* lowercase is signature */
enum {
	DUMMY_EVENT_empty = 0,
	DUMMY_EVENT_i,
	DUMMY_EVENT_s,
};

enum {
	DUMMY_REQUEST_empty = 0,
	DUMMY_REQUEST_i,
	DUMMY_REQUEST_s,
};

/* when event/request is invoked, save it here */
static unsigned short events_ackn[EVENTS_NO] = {0};
static unsigned short requests_ackn[REQUESTS_NO] = {0};

/* ----------------------------------
 *  Dummy methods
 * -------------------------------- */
/* == Requests */
static void
request_empty(struct wl_client *client, struct wl_resource *resource)
{
	assert(client);
	assert(resource);

	requests_ackn[DUMMY_REQUEST_empty]++;

	struct display *d = wl_resource_get_user_data(resource);

	assertf(d->data == resource, "Resource differs");
	wl_resource_post_event(d->data, DUMMY_EVENT_empty);
}

static void
request_i(struct wl_client *client, struct wl_resource *resource, int i)
{
	assert(client);
	assert(resource);

	assert(i == 13);
	requests_ackn[DUMMY_REQUEST_i]++;
	wl_resource_post_event(resource, DUMMY_REQUEST_i, 13);
}

static void
request_s(struct wl_client *client, struct wl_resource *resource,
	  const char *s)
{
	assert(client);
	assert(resource);

	assert(strcmp(s, "deadbee") == 0);
	requests_ackn[DUMMY_REQUEST_s]++;
	wl_resource_post_event(resource, DUMMY_REQUEST_s, s);
}


const struct wl_dummy_interface dummy_implementation = {
	request_empty,
	request_i,
	request_s,
};

/* == Events */
static void
event_empty(void *data, struct wl_dummy *dummy)
{
	assert(dummy);
	assertf(data == dummy,
		"Data set in wl_proxy_add_listener has changed");
	events_ackn[DUMMY_EVENT_empty]++;
}

static void
event_i(void *data, struct wl_dummy *dummy, int i)
{
	assert(data);
	assert(dummy);

	assertf(i == 13, "Got wrong integer value");
	events_ackn[DUMMY_EVENT_i]++;
}

static void
event_s(void *data, struct wl_dummy *dummy, const char *s)
{
	assert(data);
	assert(dummy);

	assertf(strcmp(s, "deadbee") == 0,
		"String sent to the request differs from the one catched. "
		"Should have 'deadbee' but have '%s'", s);
	events_ackn[DUMMY_EVENT_s]++;
}


static const struct wl_dummy_listener dummy_listener = {
	event_empty,
	event_i,
	event_s,
};

/* --
 * Registry binding, listeners etc..
 * -- */
static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	struct wl_dummy *dummy = NULL;

	if (strcmp(interface, "wl_dummy") == 0) {
		dummy = wl_registry_bind(registry, id,
						&wl_dummy_interface, version);
		assertf(dummy, "Binding to a registry for wl_dummy failed");

		*((struct wl_dummy **) data) = dummy;

		wl_proxy_add_listener((struct wl_proxy *) dummy,
					(void *)&dummy_listener, dummy);
	}
}

const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	NULL
};

/*
 * Basic simple test: each request will invoke an event with the very same
 * arguments. So only test if request was invoked via wl_proxy_marshal and
 * if it was catched and if it had the same arguments */
static int
proxy_marshal_main(int sock)
{
	struct wl_dummy *dummy = NULL;
	int i;

	assert(sock >= 0); /* unused variable warrning */
	struct wl_display *d = wl_display_connect(NULL);
	assert(d);

	struct wl_registry *reg = wl_display_get_registry(d);
	assert(reg);

	wl_registry_add_listener(reg, &registry_listener, &dummy);
	wl_display_dispatch(d);

	assertf(dummy, "Proxy has not been created");

	wl_proxy_marshal((struct wl_proxy *) dummy, DUMMY_REQUEST_empty);
	wl_proxy_marshal((struct wl_proxy *) dummy, DUMMY_REQUEST_i, 13);
	wl_proxy_marshal((struct wl_proxy *) dummy, DUMMY_REQUEST_s, "deadbee");

	wl_display_roundtrip(d);

	assertf(wl_display_get_error(d) == 0,
		"Error in display occured!");

	for (i = 0; i < EVENTS_NO; i++)
		assertf(events_ackn[i] == 1,
			"Event no. %d was catched %d times", i, events_ackn[i]);

	wl_proxy_destroy((struct wl_proxy *) dummy);
	wl_display_disconnect(d);

	return EXIT_SUCCESS;
}

static void
dummy_bind(struct wl_client *c, void *data, uint32_t ver, uint32_t id)
{
	struct display *d = data;
	assert(c == d->wl_client);

	/* create dummy resources */
	d->data = wl_resource_create(d->wl_client, &wl_dummy_interface, ver, id);
	assertf(d->data, "Failed creating resource for dummy");
	wl_resource_set_implementation(d->data, &dummy_implementation, d, NULL);
}

static int
run_compositor_with_dummy(int (*client_main)(int))
{
	struct display *d = display_create(NULL);
	display_create_client(d, client_main);

	struct wl_global *dummy_global = wl_global_create(d->wl_display,
							  &wl_dummy_interface,
							  1, d, dummy_bind);
	display_run(d);

	wl_global_destroy(dummy_global);
	display_destroy(d);
}

TEST(dummy_invoke_catch)
{
	int i;
	run_compositor_with_dummy(proxy_marshal_main);

	/* check request-events state */
	for (i = 0; i < REQUESTS_NO; i++)
		assertf(requests_ackn[i] == 1,
			"Request no. %d was invoked %d times", i, requests_ackn[i]);
}

#define fail(cond) if ((!(cond))) return 0
static int
proxy_marshal_wrong_opcode(int sock)
{
	struct wl_dummy *dummy = NULL;
	int stat;

	struct wl_display *d = wl_display_connect(NULL);
	/* this is fail test, 0 means failure */
	fail(d);

	struct wl_registry *reg = wl_display_get_registry(d);
	fail(reg);

	/* add address to dummy as a data and in registry global
	 * it will create the proxy */
	wl_registry_add_listener(reg, &registry_listener, &dummy);
	wl_display_dispatch(d);
	if(!dummy) {
		dbg("Proxy has not been created");
		return 0;
	}

	/* XXX nowadays client get SIGSEGV when wl_debug=1
	 * (within wl_closure_print iirc)
	 * is it correct to let client get SIGSEGV? Isn't better to
	 * check it and post error so that display can close connection
	 * correctly? */
	wl_proxy_marshal((struct wl_proxy *) dummy, 0);//REQUESTS_NO + 1);
	wl_proxy_marshal((struct wl_proxy *) dummy, REQUESTS_NO + 1);

	wl_display_roundtrip(d);
	stat = wl_display_get_error(d);
	ifdbg(stat, "Got error from display: %d\n", stat);

	wl_proxy_destroy((struct wl_proxy *) dummy);
	wl_display_disconnect(d);

	/* stat should be non-null */
	return stat;
}

FAIL_TEST(proxy_marshal_wrong_opcode_tst)
{
	run_compositor_with_dummy(proxy_marshal_wrong_opcode);
}

static int
same_ids_main(int sock)
{
	struct wl_display *d;
	struct wl_proxy *p1;

	assert(sock >= 0);
	d = wl_display_connect(NULL);
	assert(d);

	p1 = wl_proxy_create((struct wl_proxy *) d,
			     &wl_registry_interface);

	wl_proxy_marshal((struct wl_proxy *) d,
			 WL_DISPLAY_GET_REGISTRY, p1);

	/* this one should break display, because
	 * we're trying to register again p1 (the same id) */
	wl_proxy_marshal((struct wl_proxy *) d,
			 WL_DISPLAY_GET_REGISTRY, p1);

	wl_display_roundtrip(d);
	assert(wl_display_get_error(d) == 0);

	wl_proxy_destroy(p1);
	wl_display_disconnect(d);

	return EXIT_SUCCESS;
}

FAIL_TEST(same_ids)
{
	struct display *d = display_create_and_run(NULL, same_ids_main);
	display_destroy(d);
}


static int
proxy_create_main(int sock)
{
	assert(sock >= 0);
	struct wl_display *d = wl_display_connect(NULL);
	assert(d);

	struct wl_proxy *p1;
	p1 = wl_proxy_create((struct wl_proxy *) d,
			      &wl_dummy_interface);

	assert(p1);
	wl_proxy_set_user_data(p1, (void *) 0xbee);
	assertf(wl_proxy_get_user_data(p1) == (void *) 0xbee,
		"Wrong user data in proxy");

	wl_proxy_destroy(p1);

	return EXIT_SUCCESS;
}

TEST(create_setget)
{
	struct display *d = display_create_and_run(NULL, proxy_create_main);
	display_destroy(d);
}
