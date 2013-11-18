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

static void
registry_handle_global(void *data, struct wl_registry *registry,
			uint32_t id, const char *interface, uint32_t version)
{
	assert(data && registry && interface);
	struct client *cl = data;
	uint32_t *max_id = cl->data;

	/* get maximal global id */
	if (*max_id < id)
		*max_id = id;
}

const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	NULL
};

static int
global_bind_wrong_id_main(int s)
{
	int stat;
	uint32_t max_id = 0;
	struct client c;

	client_init(&c, s);
	c.data = &max_id;

	c.registry.proxy = (struct wl_proxy *) wl_display_get_registry(c.display);
	assert(c.registry.proxy);
	wl_registry_add_listener((struct wl_registry *) c.registry.proxy,
				 &registry_listener, &c);
	wl_display_dispatch(c.display);

	/* try bind to invalid global */
	struct wl_compositor *comp =
		wl_registry_bind((struct wl_registry *) c.registry.proxy,
				 max_id + 1, &wl_compositor_interface,
				 wl_compositor_interface.version);
	/* should get display error now */
	wl_display_roundtrip(c.display);
	stat = wl_display_get_error(c.display);

	if (comp)
		wl_compositor_destroy(comp);
	wl_registry_destroy((struct wl_registry *) c.registry.proxy);
	wl_display_disconnect(c.display);

	/* if we got error, everything's alright */
	return !stat;
}

/*
 * test binding to non-existing global
 */
TEST(global_bind_wrong_id_tst)
{
	struct display *d
		= display_create_and_run(NULL, global_bind_wrong_id_main);
	display_destroy(d);
}

static int
create_more_same_singletons_main(int s)
{
	struct client *c = client_populate(s);

	/* let display create global */
	client_barrier(c);
	/* wait for finishing creation of globals */
	wl_display_roundtrip(c->display);

	client_free(c);
	return EXIT_SUCCESS;
}

TEST(create_more_same_singletons_tst)
{
	struct wl_global *g1, *g2;

	/* create only display */
	struct display *d = display_create(&zero_config);
	display_create_client(d, create_more_same_singletons_main);
	display_run(d);

	g1 = wl_global_create(d->wl_display, &wl_display_interface,
			      wl_display_interface.version, NULL, NULL);
	g2 = wl_global_create(d->wl_display, &wl_display_interface,
			      wl_display_interface.version, NULL, NULL);
	display_barrier(d);

	/*
	   XXX Asked about that on IRC and I was told it's *not* a bug.
	   Althouhg, I think that when the object is stated to be singleton,
	   it should BE singletol, therefore I left the code here.
	   NOTE: I changed assert to ifdbg for now
	 */
	ifdbg(g1 || g2,
		"Display is stated a singleton but it's possible to create it "
		"more times.\n");

	wl_global_destroy(g1);
	wl_global_destroy(g2);
	display_destroy(d);
}

TEST(create_wrong_version_global_tst)
{
	struct wl_global *g;
	struct display *d = display_create(&zero_config);

	g = wl_global_create(d->wl_display, &wl_compositor_interface,
			      wl_compositor_interface.version + 1, NULL, NULL);
	assertf(g == NULL,
		"Global created even with wrong version");

	display_destroy(d);
}
