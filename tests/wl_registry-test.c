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
#include <string.h>

#include "wayland-client.h"
#include "wayland-server.h"

#include "test-runner.h"
#include "test-compositor.h"

/* -----------------------------------------------------------------------------
    Seat listener
   -------------------------------------------------------------------------- */
static void
seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name)
{
	assert(data && wl_seat && name);

	struct client *c = data;
	int *destroyed = c->data;

	assertf(*destroyed == 0,
		"event emitted even after destroying global");
}

static void
seat_handle_caps(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
	assert(data && seat);
}

const struct wl_seat_listener seat_listener = {
	seat_handle_caps,
	seat_handle_name
};


/* -----------------------------------------------------------------------------
    Registry listener
   -------------------------------------------------------------------------- */
static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	struct client *cl = data;
	int *destroyed = cl->data;
	if (strcmp(interface, "wl_seat") == 0) {
		assertf(*destroyed == 0, "Seat present after removing global");

		cl->seat.proxy = wl_registry_bind(registry, id,
						  &wl_seat_interface, version);
		assertf(cl->seat.proxy, "Binding to registry for seat failed");

		client_add_listener(cl, "wl_seat", &seat_listener);
		assertf(cl->seat.listener, "Failed adding listener");

		wl_display_roundtrip(cl->display);
		assertf(wl_display_get_error(cl->display) == 0,
			"An error in display occured");
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t id)
{
	assert(data && registry);

	struct client *c = data;
	int *destroyed = c->data;

	*destroyed = 1;
}

const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove,
};

static int
global_remove_main(int s)
{
	int destroyed = 0;
	struct client c;

	client_init(&c, s);
	c.data = &destroyed;

	c.registry.proxy = (struct wl_proxy *) wl_display_get_registry(c.display);
	assert(c.registry.proxy);
	wl_registry_add_listener((struct wl_registry *) c.registry.proxy,
				 &registry_listener, &c);
	wl_display_dispatch(c.display);

	assert(c.seat.proxy);

	/* stop client so that it can remove global */
	client_barrier(&c);
	wl_display_roundtrip(c.display);

	assertf(destroyed == 1,
		"Global destroy method haven't been called");

	/*
	 * DOC: The object remains valid and requests to the object will be
	 * ignored until the client destroys it, to avoid races between the
	 * global going away and a client sending a request to it.
	 * See mailing list:
	 * http://lists.freedesktop.org/archives/wayland-devel/2013-October/011573.html
	 */
	struct wl_touch *touch;
	touch = wl_seat_get_touch((struct wl_seat *) c.seat.proxy);
	assertf(touch == NULL,
		"Seat was already deleted, but requeset has not been ignored");

	/* try to get the globals again and check if seat's not there
	 * (handle_global method with seat interface would abort) */
	wl_registry_destroy((struct wl_registry *) c.registry.proxy);
	c.registry.proxy = (struct wl_proxy *) wl_display_get_registry(c.display);
	assert(c.registry.proxy);
	wl_display_dispatch(c.display);

	/* let display try emit events for seat */
	client_barrier(&c);

	wl_seat_destroy((struct wl_seat *) c.seat.proxy);
	wl_registry_destroy((struct wl_registry *) c.registry.proxy);

	wl_display_roundtrip(c.display);
	wl_display_disconnect(c.display);
	return EXIT_SUCCESS;
}

/*
 * test global remove event
 */
TEST(global_remove_tst)
{
	struct display *d
		= display_create_and_run(NULL, global_remove_main);

	wl_seat_send_name(d->resources.wl_seat, "Cool name");
	wl_seat_send_name(d->resources.wl_seat, "Cool name2");

	dbg("Deleting global\n");
	wl_global_destroy(d->globals.wl_seat);
	display_barrier(d);

	wl_seat_send_name(d->resources.wl_seat, "destroy");
	wl_seat_send_name(d->resources.wl_seat, "destroy1");
	wl_seat_send_name(d->resources.wl_seat, "destroy2");
	display_barrier(d);

	display_destroy(d);
}

static void
registry_handle_global3(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	struct client *cl = data;
	uint32_t *created = cl->data;

	if (strcmp(interface, "wl_compositor") == 0)
		*created = 1;
}

static const struct wl_registry_listener registry_listener3 = {
	registry_handle_global3,
	NULL
};

static int
global_main(int s)
{
	uint32_t created = 0;

	struct client c;
	client_init(&c, s);
	c.data = &created;

	c.registry.proxy = (struct wl_proxy *) wl_display_get_registry(c.display);
	assert(c.registry.proxy);
	wl_registry_add_listener((struct wl_registry *) c.registry.proxy,
				 &registry_listener3, &c);
	wl_display_dispatch(c.display);

	client_barrier(&c);
	wl_display_roundtrip(c.display);
	assertf(created == 1,
		"New global haven't been announced");

	wl_registry_destroy((struct wl_registry *) c.registry.proxy);
	wl_display_disconnect(c.display);
	return EXIT_SUCCESS;
}

/* test global event (if it's announced after creation) */
TEST(global_tst)
{
	struct wl_global *g;
	struct config conf = {0,0,0};
	struct display *d = display_create(&conf);
	display_create_client(d, global_main);
	display_run(d);

	g = wl_global_create(d->wl_display, &wl_compositor_interface,
			     wl_compositor_interface.version,
			     NULL, NULL);
	assert(g);
	display_barrier(d);

	wl_global_destroy(g);
	display_destroy(d);
}
