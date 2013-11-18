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

#include "test-runner.h"
#include "test-compositor.h"

TEST(compositor_create)
{
	/* since I plan adding objects, keep here last default config
	 * and check, if default config have changed since last time.
	 * So that I know that the tests are out of date */
	const struct config old_default_config = {
		CONF_SEAT | CONF_COMPOSITOR,
		CONF_ALL,
		0
	};

	struct display *d = display_create(NULL);

	/* check configuration version */
	assertf(old_default_config.globals == d->config.globals &&
		old_default_config.resources == d->config.resources &&
		old_default_config.options == d->config.options,
		"Config tests are out of date. Default config changed");

	/* Following tests should be covered by asserts in code, but what if I
	 * forget or will change something? */
	assertf(d->wl_display, "Display wasn't created");
	assertf(d->wl_client == NULL, "Client is not NULL before calling client_create");
	assertf(d->sigchld, "Event source (SIGCHLD signal) is NULL");
	assertf(d->sigusr1, "Event source (SIGUSR1 signal) is NULL");
	assertf(d->loop, "Got no event loop");
	assertf(d->client_pid == 0, "Client pid is set even though we "
					"haven't created client yet");
	assertf(d->client_exit_code == 0, "Client exit code differs from 0 "
						"after initizalization");

	assertf(d->data == NULL, "Client non-NULL before setting");
	assertf(d->user_func == NULL, "User func is NULL before setting");

	assert(d->request == 0);

	/* check for default configs */
	assert(d->config.globals == (CONF_SEAT | CONF_COMPOSITOR));
	assert(d->config.resources == CONF_ALL);
	assert(d->config.options == ~CONF_ALL);
	/* ~CONF_ALL == 0 */
	assert(~CONF_ALL == 0); /* sanity test */

	display_destroy(d);
}

static int
client_main(int s)
{
	assert(s >= 0);
	return 42;
}

TEST(client_create)
{
	struct display *d = display_create(NULL);
	display_create_client(d, client_main);

	assertf(d->wl_client, "Client is NULL");
	assertf(d->client_pid != 0, "Client pid is weird (%d)..", d->client_pid);

	display_run(d);
	assertf(d->client_exit_code == 42,
		"The value returned in client_main doesn't mach 42 (%d)",
		d->client_exit_code);

	/* nobody requested anything */
	assert(d->request == 0);

	/* display checks for 0 */
	d->client_exit_code = 0;

	display_destroy(d);
}

TEST(client_create_another_way)
{
	struct display *d = display_create_and_run(NULL, client_main);
	assertf(d->client_exit_code == 42,
		"The value returned in client_main doesn't mach 42 (%d)",
		d->client_exit_code);

	d->client_exit_code = 0;
	display_destroy(d);
}

TEST(user_data_without_destr)
{
	struct display *d = display_create(NULL);
	/* program shouldn't crash with NULL destructor */
	display_add_user_data(d, (void *) 0xbee, NULL);
	assertf(display_get_user_data(d) == (void *) 0xbee,
		"Got %p instead of 0xbee", display_get_user_data(d));

	display_destroy(d);
}

static _Bool destroy_bee_called = 0;
static void
destroy_bee(void *data)
{
	destroy_bee_called = 1;
	assertf(data == (void *) 0xbee,
		"Passed wrong data in data's destructor");
}

TEST(user_data_with_destr)
{
	struct display *d = display_create(NULL);
	display_add_user_data(d, (void *) 0xbee, destroy_bee);
	display_destroy(d);

	assertf(destroy_bee_called, "Destructor wasn't called");
}

static _Bool user_func_called = 0;
static void
user_func(void *data)
{
	user_func_called = 1;
	assertf(data = (void *) 0xdeadbee,
		"data should be 0xdeadbee but is %p", data);
}

static int
user_func_main(int sock)
{
	struct client *c = client_populate(sock);

	client_call_user_func(c);

	client_free(c);
	return EXIT_SUCCESS;
}

TEST(user_func_tst)
{
	struct display *d = display_create(NULL);

	display_create_client(d, user_func_main);
	display_add_user_func(d, user_func, (void *) 0xdeadbee);

	display_run(d);

	/* process user func */
	display_run_user_func(d);

	assertf(user_func_called, "User function wasn't called");

	display_destroy(d);
}

TEST(config_tst)
{
	struct config conf = {
		.globals = CONF_SEAT,
		.resources = CONF_ALL
	};

	struct display *d = display_create(&conf);

	assert(d->config.globals == CONF_SEAT);
	assert(d->config.resources == CONF_ALL);
	assert(d->config.options == ~CONF_ALL);

	display_destroy(d);
}

static int
client_populate_main(int sock)
{
	struct client *c = client_populate(sock);
	assert(c);
	assert(c->display);
	assert(c->registry.proxy);

	assert(c->compositor.proxy);
	assert(c->seat.proxy);
	assert(c->pointer.proxy);
	assert(c->keyboard.proxy);
	assert(c->touch.proxy);
	assert(c->shm.proxy);

	wl_display_roundtrip(c->display);

	client_free(c);

	return EXIT_SUCCESS;
}

TEST(client_populate_tst)
{
	struct config conf = {CONF_ALL, CONF_ALL, 0};
	struct display *d = display_create(&conf);
	display_create_client(d, client_populate_main);

	display_run(d);

	/* we have default settings */
	assert(d->globals.wl_compositor);
	assert(d->globals.wl_seat);
	assert(d->globals.global == NULL);
	assert(d->resources.wl_compositor);
	assert(d->resources.wl_seat);
	assert(d->resources.wl_pointer);
	assert(d->resources.wl_keyboard);
	assert(d->resources.wl_touch);
	display_destroy(d);
}

static const struct wl_pointer_listener *dummy_pointer_listener = (void *) 0xBED;
static const struct wl_keyboard_listener *dummy_keyboard_listener = (void *) 0xB00;
static const struct wl_touch_listener *dummy_touch_listener = (void *) 0xBEAF;

static int
add_listener_main(int sock)
{
	struct client *c = client_populate(sock);

	assertf(c->registry.listener != NULL,
		"In populate should have been default registry listener assigned");
	assertf(c->seat.listener == NULL,
		"We didn't created seat so the default seat listener shouldn't be assigned");


	client_add_listener(c, "wl_pointer",
				(void *) dummy_pointer_listener);
	assertf(c->pointer.listener == (void *) dummy_pointer_listener,
		"Failed adding pointer listener");

	client_add_listener(c, "wl_keyboard",
				(void *) dummy_keyboard_listener);
	assertf(c->keyboard.listener == (void *) dummy_keyboard_listener,
		"Failed adding keyboard listener");

	client_add_listener(c, "wl_touch",
				(void *) dummy_touch_listener);
	assertf(c->touch.listener == (void *) dummy_touch_listener,
		"Failed adding touch listener");

	client_free(c);
	return EXIT_SUCCESS;
}

TEST(add_listener_tst)
{
	/* don't create resources, otherwise we'll get SIGSEGV */
	struct display *d = display_create(&zero_config);
	display_create_client(d, add_listener_main);

	display_run(d);

	display_destroy(d);
}

FAIL_TEST(add_unknown_interface_listener_tst)
{
	struct client c;
	memset(&c, 0, sizeof c);

	/* should abort from inside the function */
	client_add_listener(&c, "wl_unknown_interface_!@#$",
				(void *) dummy_pointer_listener);

	/* only print, assert would pass the test, because this test
	 * is expected to fail */
	fprintf(stderr, "We should have been aborted by now...");
}

static int
test_operations_main(int sock)
{
	struct client *c = client_populate(sock);
	char str[] = "I'm bytestream";
	size_t size;

	client_send_data(c, &str, sizeof(str));

	char *dstr = malloc(sizeof(str));
	client_recieve_data(c, &dstr, &size);
	assertf(strcmp(str, dstr) == 0,
		"Recieved string differs from original string: '%s' != '%s'",
		str, dstr);
	assertf(size == sizeof(str), "Got wrong size\n");

	wl_display_roundtrip(c->display);
	free(dstr);
	client_free(c);
	return EXIT_SUCCESS;
}

TEST(test_operations_tst)
{
	struct display *d = display_create(&zero_config);
	display_create_client(d, test_operations_main);

	display_run(d);

	display_recieve_data(d);
	assert(strcmp(d->data, "I'm bytestream") == 0);

	display_send_data(d, d->data, strlen(d->data) + 1);

	display_destroy(d);
}


