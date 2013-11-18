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
    Callback listener
   -------------------------------------------------------------------------- */
static void
callback_handle_done(void *data, struct wl_callback *callback, uint32_t serial)
{
	uint32_t *cserial = ((struct client *) data)->data;

	/* I haven't raised any request to the display after sync,
	 * so the serials should be the same */
	assertf(*cserial == serial, "Different serial (%u and %u)",
		*cserial, serial);

	/* By increasing the value of cserial acknowledge invocation of
	 * callback handler */
	(*cserial)++;
}

static const struct wl_callback_listener callback_listener = {
	callback_handle_done
};

/**
 * Doc: A.1.1.1. wl_display::sync - asynchronous roundtrip
 */
static int
callback_main(int s)
{
	uint32_t *serial1, serial2;
	struct client c;

	client_init(&c, s);

	client_recieve_data(&c, &serial1, NULL);
	c.data = serial1;
	serial2 = *serial1;

	struct wl_callback *cb = wl_display_sync(c.display);
	assert(cb);

	wl_callback_add_listener(cb, &callback_listener, &c);
	wl_display_dispatch(c.display);

	assertf(*serial1 == serial2 + 1, "Callback hasn't been called");

	free(serial1);
	wl_callback_destroy(cb);
	client_deinit(&c);
	return EXIT_SUCCESS;
}

TEST(callback_tst)
{
	uint32_t serial;
	struct display *d = display_create_and_run(&zero_config, callback_main);

	serial = wl_display_get_serial(d->wl_display);
	display_send_data(d, &serial, sizeof serial);

	display_destroy(d);
}

/**
 * Doc: A.1.1.2. wl_display::get_registry - get global registry object
 */
static int
get_registry_main(int s)
{
	struct wl_display *d = wl_display_connect(NULL);
	assert(d);

	struct wl_registry *r = wl_display_get_registry(d);
	wl_display_dispatch(d);
	assert(r);

	wl_registry_destroy(r);
	wl_display_disconnect(d);
	return EXIT_SUCCESS;
}

TEST(get_registry_tst)
{
	display_destroy(display_create_and_run(&zero_config, get_registry_main));
}

/**
 * Doc: A.1.2.1. wl_display::error - fatal error event
 */
static int
display_error_main(int s)
{
	struct client *c = client_populate(s);

	client_barrier(c);
	wl_display_dispatch(c->display);

	/* let client_free catch error */
	client_free(c);
	return EXIT_SUCCESS;
}

FAIL_TEST(display_error_tst)
{
	/* create at least compositor */
	struct config conf = {CONF_COMPOSITOR, CONF_COMPOSITOR, 0};
	struct display *d
		= display_create_and_run(&conf, display_error_main);

	assert(d->resources.wl_compositor);
	wl_resource_post_error(d->resources.wl_compositor,
			       WL_DISPLAY_ERROR_INVALID_METHOD,
			       "Terrible error!");
	dbg("Error posted\n");
	display_barrier(d);

	display_destroy(d);
}

/* -----------------------------------------------------------------------------
    Registry listener
static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t id)
{
	assertf(0, "Why is this called?");
}

const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove,
};
   -------------------------------------------------------------------------- */
