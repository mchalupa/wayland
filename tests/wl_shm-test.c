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
handle_format1(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	assert(data && wl_shm);

	struct client *c = data;
	int *n = c->data;

	(*n)++;
}

static const struct wl_shm_listener format_listener = {
	.format = handle_format1
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	struct client *cl = data;

	if (strcmp(interface, "wl_shm") == 0) {
		cl->shm.proxy = wl_registry_bind(registry, id,
						 &wl_shm_interface,
						 version);
		assertf(cl->shm.proxy,
			"Binding to registry for wl_shm failed");
		client_add_listener(cl, "wl_shm", (void *) &format_listener);

		wl_display_roundtrip(cl->display);
		assertf(wl_display_get_error(cl->display) == 0,
			"An error in display occured");
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global
};

/*
 * DOC: At connection setup time, the wl_shm object emits **one or more** format
 * events to inform clients about the valid pixel formats that can be used
 * for buffers.
 */
static int
format_emit_main(int s)
{
	int called_no = 0;
	struct client c;
	client_init(&c, s);

	c.data = &called_no;
	assert(c.display);

	c.registry.proxy = (struct wl_proxy *) wl_display_get_registry(c.display);
	client_add_listener(&c, "wl_registry", (void *) &registry_listener);
	assert(c.registry.proxy);
	wl_display_dispatch(c.display);

	assertf(called_no > 0, "No format emitted (no: %d)", called_no);

	wl_registry_destroy((struct wl_registry *) c.registry.proxy);
	wl_display_disconnect(c.display);
	return EXIT_SUCCESS;
}

TEST(format_emit_tst)
{
	struct config conf = {CONF_SHM,CONF_SHM,0};
	struct display *d = display_create_and_run(&conf, format_emit_main);
	display_destroy(d);
}
