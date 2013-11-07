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

#include <string.h>

#include "wayland-server.h"
#include "wayland-client.h"

#include "test-runner.h"
#include "test-compositor.h"

TEST(define_event_tst)
{
	EVENT_DEFINE(pointer_motion, &wl_pointer_interface, WL_POINTER_MOTION);
	EVENT_DEFINE(touch_frame, &wl_touch_interface, WL_TOUCH_FRAME);

	/* pointer_motion and touch_frame structures should be accessible now */
	/* otherwise it fails during compilation */
	assert(pointer_motion->interface == &wl_pointer_interface);
	assert(touch_frame->interface == &wl_touch_interface);

	/* methods */
	assertf(strcmp(pointer_motion->interface->events[pointer_motion->opcode].name,
		"motion") == 0, "Wrong method assigend to pointer_motion");
	assertf(strcmp(touch_frame->interface->events[touch_frame->opcode].name,
		"frame") == 0, "Wrong method assigend to touch_frame");


}

static const struct wl_interface intf = {"wl_test_interface", 1, 0, NULL, 3, NULL};

TEST(define_edge_event_opcode_tst)
{
	/* opcode is the lower edge value */
	EVENT_DEFINE(event, &intf, 2);
	exit(! event->interface); /* suppress compiler warning */
}

FAIL_TEST(define_illegal_event_1_tst)
{
	/* opcode is the higher edge value */
	EVENT_DEFINE(event, &intf, 3);
	exit(! event->interface); /* suppress compiler warning */
}

FAIL_TEST(define_illegal_event_2_tst)
{
	/* too big opcode */
	EVENT_DEFINE(event, &intf, 4);
	exit(! event->interface); /* suppress compiler warning */
}

/* the compilation will fail if this macro is wrong */
EVENT_DEFINE_GLOBAL(anevent, &wl_pointer_interface,
			WL_POINTER_MOTION);
TEST(define_global_event)
{
	/* define the same event locally and compare */
	EVENT_DEFINE(othevent, &wl_pointer_interface,
			 WL_POINTER_MOTION);
	assertf(anevent->interface == othevent->interface,
		"Interfaces differs");
	assertf(anevent->opcode == othevent->opcode,
		"Opcodes differs");
	assertf(strcmp(anevent->interface->events[anevent->opcode].name,
			othevent->interface->events[othevent->opcode].name) == 0,
		"Events have different methods");
}

TEST(eventarray_init_tst)
{
	int i;

	/* tea = test event array, but you probably know what I was drinking
	 * at the moment :) */
	struct eventarray *tea = eventarray_create();
	struct eventarray *teabag = eventarray_create();

	for (i = 0; i < MAX_EVENTS; i++) {
		assertf(tea->events[i] == 0, "Field no. %d is not inizialized", i);
		assertf(teabag->events[i] == 0, "Field no. %d is not inizialized"
			" (teabag)", i);
	}

	assertf(tea->count == 0, "Count not initialized");
	assertf(tea->index == 0, "Index not initialized");

	eventarray_free(tea);
	eventarray_free(teabag);
}

FAIL_TEST(eventarray_add_wrong_event_tst)
{
	struct eventarray *tea = eventarray_create();
	eventarray_add(tea, 0, NULL);
	eventarray_free(tea);
}

FAIL_TEST(eventarray_add_wrong_ea_tst)
{
	EVENT_DEFINE(e, &wl_pointer_interface, WL_POINTER_MOTION);
	eventarray_add(NULL, 0, e);
}

TEST(eventarray_add_tst)
{
	unsigned count = -1;

	struct eventarray *tea = eventarray_create();
	EVENT_DEFINE(key, &wl_keyboard_interface, WL_KEYBOARD_KEY);

	count = eventarray_add(tea, DISPLAY, key, 0, 0, 0, 0);
	assertf(tea->count == 1, "Count not increased");
	assertf(tea->count == count, "Count returned wrong count");
	assertf(tea->index == 0, "Index should have not been increased");
	assertf(tea->events[0] != NULL, "Event not saved");
	assertf(tea->events[1] == NULL, "Wrong memory state");

	count = eventarray_add(tea, DISPLAY, key, 1, 1, 1, 1);
	assertf(tea->count == 2, "Count not increased");
	assertf(tea->count == count, "Count returned wrong count");
	assertf(tea->index == 0, "Index should have not been increased");
	assertf(tea->events[1] != NULL, "Event not saved");
	assertf(tea->events[2] == NULL, "Wrong memory state");

	eventarray_free(tea);
}


/* just define some events, no matter what events and do it manually, so it can
 * be global */
EVENT_DEFINE_GLOBAL(touch_e, &wl_touch_interface, WL_TOUCH_FRAME);
EVENT_DEFINE_GLOBAL(pointer_e, &wl_pointer_interface, WL_POINTER_BUTTON);
EVENT_DEFINE_GLOBAL(keyboard_e, &wl_keyboard_interface, WL_KEYBOARD_KEY);
EVENT_DEFINE_GLOBAL(seat_e, &wl_seat_interface, WL_SEAT_NAME);

static int
eventarray_emit_main(int sock)
{
	struct client *c = client_populate(sock);
	struct wl_surface *surface
			= wl_compositor_create_surface(
				(struct wl_compositor *) c->compositor.proxy);
	assert(surface);
	wl_display_roundtrip(c->display);

	client_ask_for_events(c, 0);

	wl_display_roundtrip(c->display);
	assert(strcmp(c->seat.data, "Cool name") == 0);

	wl_surface_destroy(surface);
	client_free(c);
	return EXIT_SUCCESS;
}

TEST(eventarray_emit_tst)
{
	struct eventarray *tea = eventarray_create();

	struct display *d = display_create(NULL);
	display_create_client(d, eventarray_emit_main);
	display_add_events(d, tea);

	display_run(d);

	/* XXX test all resources. It doesn't matter that we don't check anything,
	 * only test if won't get SIGSEGV or SIGABRT */
	eventarray_add(tea, DISPLAY, touch_e);
	eventarray_add(tea, DISPLAY, pointer_e, 0, 0, 0, 0);
	eventarray_add(tea, DISPLAY, keyboard_e, 0, 0, 0, 0);
	eventarray_add(tea, DISPLAY, seat_e, "Cool name");

	assert(tea->count == 4 && tea->index == 0);

	/* just test if we won't get SIGSEGV because of bad resource */
	display_emit_events(d);

	assert(tea->count == 4);
	assertf(tea->index == 4, "Index is set wrong (%d)", tea->index);

	/* XXX uncomment on release
	 * how many resources we tested?
	 * it's just for me..
	int resources_tst = tea->count + 1; // +1 = compositor doesn't have events
	int resources_no = sizeof(d->resources) / sizeof(struct wl_resource *);

	assertf(resources_tst == resources_no, "Missing tests for new resources");
	*/

	display_destroy(d);
}

TEST(eventarray_compare_tst)
{
	struct eventarray *e1 = eventarray_create();
	struct eventarray *e2 = eventarray_create();
	EVENT_DEFINE(pointer_motion, &wl_pointer_interface, WL_POINTER_MOTION);
	EVENT_DEFINE(seat_caps, &wl_seat_interface, WL_SEAT_CAPABILITIES);

	assertf(eventarray_compare(e1, e1) == 0,
		"The same eventarrays are not equal");
	assertf(eventarray_compare(e1, e2) == 0
		&& eventarray_compare(e2, e1) == 0,
		"Empty eventarrays are not equal");

	eventarray_add(e1, DISPLAY, pointer_motion, 1, 2, 3, 4);
	eventarray_add(e2, DISPLAY, pointer_motion, 1, 2, 3, 4);
	assertf(eventarray_compare(e1, e2) == 0);

	eventarray_add(e1, DISPLAY, seat_caps, 4);
	assertf(eventarray_compare(e1, e2) != 0);
	assertf(eventarray_compare(e2, e1) != 0);

	eventarray_add(e2, DISPLAY, seat_caps, 4);
	assertf(eventarray_compare(e2, e1) == 0);
	assertf(eventarray_compare(e1, e2) == 0);

	eventarray_add(e2, DISPLAY, pointer_motion,  0, 0, 0);
	assertf(eventarray_compare(e1, e2) != 0);

	eventarray_add(e1, DISPLAY, pointer_motion,  0, 0, 0);
	assertf(eventarray_compare(e1, e2) == 0);

	assertf(eventarray_compare(e1, e1) == 0);
	assertf(eventarray_compare(e2, e2) == 0);

	eventarray_free(e1);
	eventarray_free(e2);
}

/* test adding arguments which are dynamically allocated:
 * string and array */
TEST(ea_add_dynamic)
{

	EVENT_DEFINE(seat_name, &wl_seat_interface, WL_SEAT_NAME);
	EVENT_DEFINE(keyboard_enter, &wl_keyboard_interface, WL_KEYBOARD_ENTER);

	/* try string */
	struct eventarray *ea = eventarray_create();
	eventarray_add(ea, DISPLAY, seat_name, "Cool name");
	/* test-runner will assert on leaked memory */
	eventarray_free(ea);

	/* try array */
	ea = eventarray_create();

	/* I need a proxy for keyboard enter */
	struct config conf = {0,0,0};
	struct display *d = display_create(&conf);

	struct wl_array a;
	wl_array_init(&a);
	wl_array_add(&a, 10);
	strcpy(a.data, "Cool array");

	eventarray_add(ea, DISPLAY, keyboard_enter,
			   0x5e41a1, d->wl_display, &a);

	eventarray_free(ea);

	/* try both */
	ea = eventarray_create();

	eventarray_add(ea, DISPLAY, seat_name, "Cool name");
	eventarray_add(ea, DISPLAY, keyboard_enter, 0x5e41a1, d->wl_display, &a);

	display_destroy(d);
	wl_array_release(&a);
	eventarray_free(ea);
}


/* test events which don't allocate their own memory */
static int
send_ea_basic_main(int sock)
{
	struct client *c = client_populate(sock);
	struct eventarray *ea = eventarray_create();

	/* try events with arguments: uint32_t, int32_t, wl_fixed_t and
	 * object (it will be send as id */
	EVENT_DEFINE(touch_motion, &wl_touch_interface, WL_TOUCH_MOTION);

	eventarray_add(ea, CLIENT, touch_motion, 0x0131, -5,
			   wl_fixed_from_int(45), wl_fixed_from_double(2.74));
	eventarray_add(ea, CLIENT, touch_motion, 0x0131, -5,
			   wl_fixed_from_int(45), wl_fixed_from_double(2.74));
	eventarray_add(ea, CLIENT, touch_motion, 0x0131, -5,
			   wl_fixed_from_int(45), wl_fixed_from_double(2.74));

	client_send_eventarray(c, ea);
	client_ask_for_events(c, 3);

	eventarray_free(ea);
	client_free(c);

	return EXIT_SUCCESS;
}

TEST(send_eventarray_basic_events_tst)
{
	struct eventarray *ea = eventarray_create();
	struct display *d = display_create(NULL);

	display_create_client(d, send_ea_basic_main);
	display_run(d);

	display_recieve_eventarray(d);
	assert(d->events);

	EVENT_DEFINE(touch_motion, &wl_touch_interface, WL_TOUCH_MOTION);

	eventarray_add(ea, DISPLAY, touch_motion, 0x0131, -5,
			   wl_fixed_from_int(45), wl_fixed_from_double(2.74));
	eventarray_add(ea, DISPLAY, touch_motion, 0x0131, -5,
			   wl_fixed_from_int(45), wl_fixed_from_double(2.74));
	eventarray_add(ea, DISPLAY, touch_motion, 0x0131, -5,
			   wl_fixed_from_int(45), wl_fixed_from_double(2.74));

	/* try emit commited eventarray, don't catch it. Only test if we won't get
	 * some error. That the eventarray is same we'll test by compare() */
	display_emit_events(d);

	assert(eventarray_compare(d->events, ea) == 0);

	eventarray_free(ea);
	display_destroy(d);
}


static void
pointer_handle_button(void *data, struct wl_pointer *pointer, uint32_t serial,
		      uint32_t time, uint32_t button, uint32_t state)
{
	assert(data);
	assert(pointer);
	assert(serial == 0xbee);
	assert(time == 0xdead);
	assert(button == 0);
	assert(state == 1);

	struct client *c = data;
	c->data = (void *) 0xb00;
}

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
		     struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
{
	assert(data);
	assert(pointer);
	assert(serial == 0);
	assert(surface);
	assert(wl_fixed_to_int(x) == 13);
	assert(wl_fixed_to_int(y) == 43);
	assert((uint32_t) wl_proxy_get_user_data((struct wl_proxy *) surface) ==
	       wl_proxy_get_id((struct wl_proxy *) surface));

	struct client *c = data;
	(*((int *) c->data))++;
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface)
{
	assert(data && pointer);
	assert(surface);

	struct client *c = data;
	(*((int *) c->data))++;
}


static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_handle_enter,
	.leave = pointer_handle_leave,
	.button = pointer_handle_button,
};

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *array)
{
	assert(data && keyboard && surface && array);
	assert(serial == 0);

	assert(strcmp(array->data, "Cool array") == 0);

	struct client *c = data;
	(*((int *) c->data))++;
}

static const struct wl_keyboard_listener keyboard_listener = {
	.enter = keyboard_handle_enter
};

static int
send_one_event_main(int sock)
{
	struct client *c = client_populate(sock);
	client_add_listener(c, "wl_pointer", (void *) &pointer_listener);

	EVENT_DEFINE(pointer_button, &wl_pointer_interface, WL_POINTER_BUTTON);
	client_trigger_event(c, pointer_button, 0xbee, 0xdead, 0, 1);
	wl_display_dispatch(c->display);

	assert(c->data == (void *) 0xb00);

	client_free(c);
	return EXIT_SUCCESS;
}

TEST(send_one_event_tst)
{
	struct display *d = display_create(NULL);
	display_create_client(d, send_one_event_main);

	display_run(d);
	display_emit_event(d);

	display_destroy(d);
}

static int
send_one_event2_main(int sock)
{
	struct wl_surface *surf;
	struct client *c = client_populate(sock);
	client_add_listener(c, "wl_pointer", (void *) &pointer_listener);

	surf = wl_compositor_create_surface((struct wl_compositor *) c->compositor.proxy);
	wl_display_roundtrip(c->display);
	assert(surf);
	wl_surface_set_user_data(surf,
				 (void *) wl_proxy_get_id((struct wl_proxy *) surf));

	int count = 0;
	c->data = &count;

	EVENT_DEFINE(pointer_enter, &wl_pointer_interface, WL_POINTER_ENTER);
	client_trigger_event(c, pointer_enter, 0, surf,
				 wl_fixed_from_int(13), wl_fixed_from_int(43));
	wl_display_dispatch(c->display);

	assertf(count == 1, "Called only %d callback (instead of 1)", count);

	wl_surface_destroy(surf);
	client_free(c);
	return EXIT_SUCCESS;
}

TEST(send_one_event2_tst)
{
	struct display *d = display_create(NULL);
	display_create_client(d, send_one_event2_main);

	display_run(d);
	display_emit_event(d);

	display_destroy(d);
}

static int
trigger_multiple_event_main(int s)
{
	/* define events */
	EVENT_DEFINE(pointer_enter, &wl_pointer_interface, WL_POINTER_ENTER);
	EVENT_DEFINE(pointer_leave, &wl_pointer_interface, WL_POINTER_LEAVE);
	EVENT_DEFINE(keyboard_enter, &wl_keyboard_interface, WL_KEYBOARD_ENTER);

	/* get and create objects */
	struct client *c = client_populate(s);
	struct wl_array array;
	struct wl_surface *surf;
	surf = wl_compositor_create_surface((struct wl_compositor *) c->compositor.proxy);
	wl_surface_set_user_data(surf,
				 (void *) wl_proxy_get_id((struct wl_proxy *) surf));

	/* counter of callbacks called */
	int count = 0;
	c->data = &count;

	/* add listeners */
	client_add_listener(c, "wl_pointer", (void *) &pointer_listener);
	client_add_listener(c, "wl_keyboard", (void *) &keyboard_listener);

	wl_array_init(&array);
	wl_array_add(&array, 15);
	strcpy(array.data, "Cool array");

	/* trigger emitting events */
	client_trigger_event(c, pointer_enter, 0, surf,
				 wl_fixed_from_int(13), wl_fixed_from_int(43));
	client_trigger_event(c, pointer_leave, 0, surf);
	client_trigger_event(c, keyboard_enter, 0, surf, &array);
	wl_display_dispatch(c->display);

	assertf(count == 3, "Called only %d callback (instead of 3)", count);

	wl_array_release(&array);
	wl_surface_destroy(surf);
	client_free(c);
	return EXIT_SUCCESS;
}

TEST(trigger_multiple_event_tst)
{
	struct display *d
		= display_create_and_run(NULL, trigger_multiple_event_main);

	display_emit_event(d); /* enter */
	display_emit_event(d); /* leave */
	display_emit_event(d); /* keyboard enter */

	display_destroy(d);
}
