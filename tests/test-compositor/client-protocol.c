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

#include "wayland-client.h"
#include "wayland-server.h"

#include "tc-client.h"
#include "tc-utils.h"

/* -----------------------------------------------------------------------------
    Seat listener
   -------------------------------------------------------------------------- */
static void
seat_handle_caps(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
	assertf(data, "No data when catched seat");
	assertf(seat, "No seat when catched seat");

	struct client *cl = data;

	if (caps & WL_SEAT_CAPABILITY_POINTER) {
		if (cl->pointer.proxy)
			wl_pointer_destroy((struct wl_pointer *) cl->pointer.proxy);

		cl->pointer.proxy = (struct wl_proxy *) wl_seat_get_pointer(seat);
		assertf(cl->pointer.proxy,
			"wl_seat_get_pointer returned NULL in seat_listener function");

		if (cl->pointer.listener)
			wl_pointer_add_listener(
				(struct wl_pointer *) cl->pointer.proxy,
				(struct wl_pointer_listener *) cl->pointer.listener,
				cl);
	}

	if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
		if (cl->keyboard.proxy)
			wl_keyboard_destroy((struct wl_keyboard *) cl->keyboard.proxy);

		cl->keyboard.proxy = (struct wl_proxy *) wl_seat_get_keyboard(seat);
		assertf(cl->keyboard.proxy,
			"Got no keyboard from seat");

		if (cl->keyboard.listener)
			wl_keyboard_add_listener(
				(struct wl_keyboard *) cl->keyboard.proxy,
				(struct wl_keyboard_listener *) cl->keyboard.listener,
				cl);
	}

	if (caps & WL_SEAT_CAPABILITY_TOUCH) {
		if (cl->touch.proxy)
			wl_touch_destroy((struct wl_touch *) cl->touch.proxy);

		cl->touch.proxy = (struct wl_proxy *) wl_seat_get_touch(seat);
		assertf(cl->touch.proxy,
			"Got no touch from seat");

		if (cl->touch.listener)
			wl_touch_add_listener(
				(struct wl_touch *) cl->touch.proxy,
				(struct wl_touch_listener *) cl->touch.listener,
				cl);
	}

	/* block until synced, or client will end too early */
	if (caps)
		wl_display_dispatch_pending(cl->display);

	assertf(wl_display_get_error(cl->display) == 0,
		"An error in display occured");
}

static void
seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name)
{
	struct client *c = data;

	c->seat.data = strdup(name);
	c->seat.data_destr = &free;
}

const struct wl_seat_listener seat_default_listener = {
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
	if (strcmp(interface, "wl_seat") == 0) {
		if (cl->seat.proxy)
			wl_seat_destroy((struct wl_seat *) cl->seat.proxy);

		cl->seat.proxy = wl_registry_bind(registry, id,
						  &wl_seat_interface, version);
		assertf(cl->seat.proxy, "Binding to registry for seat failed");

		client_add_listener(cl, "wl_seat", &seat_default_listener);
		assertf(cl->seat.listener, "Failed adding listener");
	} else if (strcmp(interface, "wl_compositor") == 0) {
		if (cl->compositor.proxy)
			wl_compositor_destroy(
				(struct wl_compositor *) cl->compositor.proxy);

		cl->compositor.proxy = wl_registry_bind(registry, id,
							&wl_compositor_interface,
							version);
		assertf(cl->compositor.proxy,
			"Binding to registry for compositor failed");
	} else if (strcmp(interface, "wl_shm") == 0) {
		if (cl->shm.proxy)
			wl_shm_destroy((struct wl_shm *) cl->shm.proxy);

		cl->shm.proxy = wl_registry_bind(registry, id,
						 &wl_shm_interface,
						 version);
		assertf(cl->shm.proxy,
			"Binding to registry for wl_shm failed");
	} else if (strcmp(interface, "wl_display") == 0) {
		return;
	} else {
		assertf(0, "Unknown interface!");
	}

	wl_display_roundtrip(cl->display);
	assertf(wl_display_get_error(cl->display) == 0,
		"An error in display occured");
}

const struct wl_registry_listener registry_default_listener = {
	registry_handle_global,
	NULL /* TODO */
};
