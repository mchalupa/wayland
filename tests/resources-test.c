/*
 * Copyright © 2013 Marek Chalupa
 * Copyright © 2015 Red Hat, Inc.
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
#include <string.h>

#define WL_HIDE_DEPRECATED 1

#include "wayland-server.h"
#include "test-runner.h"
#include "test-compositor.h"

TEST(create_resource_tst)
{
	struct wl_display *display;
	struct wl_client *client;
	struct wl_resource *res;
	struct wl_list *link;
	int s[2];
	uint32_t id;

	assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0);
	display = wl_display_create();
	assert(display);
	client = wl_client_create(display, s[0]);
	assert(client);

	res = wl_resource_create(client, &wl_display_interface, 4, 0);
	assert(res);

	/* setters/getters */
	assert(wl_resource_get_version(res) == 4);

	assert(client == wl_resource_get_client(res));
	id = wl_resource_get_id(res);
	assert(wl_client_get_object(client, id) == res);

	link = wl_resource_get_link(res);
	assert(link);
	assert(wl_resource_from_link(link) == res);

	wl_resource_set_user_data(res, (void *) 0xbee);
	assert(wl_resource_get_user_data(res) == (void *) 0xbee);

	assert(!wl_resource_is_inert(res));
	wl_resource_set_inert(res);
	assert(wl_resource_is_inert(res));

	wl_resource_destroy(res);
	wl_client_destroy(client);
	wl_display_destroy(display);
	close(s[1]);
}

static void
res_destroy_func(struct wl_resource *res)
{
	assert(res);

	_Bool *destr = wl_resource_get_user_data(res);
	*destr = 1;
}

static _Bool notify_called = 0;
static void
destroy_notify(struct wl_listener *l, void *data)
{
	assert(l && data);
	notify_called = 1;
}

TEST(destroy_res_tst)
{
	struct wl_display *display;
	struct wl_client *client;
	struct wl_resource *res;
	int s[2];
	unsigned id;
	struct wl_list *link;

	_Bool destroyed = 0;
	struct wl_listener destroy_listener = {
		.notify = &destroy_notify
	};

	assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0);
	display = wl_display_create();
	assert(display);
	client = wl_client_create(display, s[0]);
	assert(client);

	res = wl_resource_create(client, &wl_display_interface, 4, 0);
	assert(res);
	wl_resource_set_implementation(res, NULL, &destroyed, res_destroy_func);
	wl_resource_add_destroy_listener(res, &destroy_listener);

	id = wl_resource_get_id(res);
	link = wl_resource_get_link(res);
	assert(link);

	wl_resource_destroy(res);
	assert(destroyed);
	assert(notify_called); /* check if signal was emitted */
	assert(wl_client_get_object(client, id) == NULL);

	res = wl_resource_create(client, &wl_display_interface, 2, 0);
	assert(res);
	destroyed = 0;
	notify_called = 0;
	wl_resource_set_destructor(res, res_destroy_func);
	wl_resource_set_user_data(res, &destroyed);
	wl_resource_add_destroy_listener(res, &destroy_listener);
	/* client should destroy the resource upon its destruction */
	wl_client_destroy(client);
	assert(destroyed);
	assert(notify_called);

	wl_display_destroy(display);
	close(s[1]);
}

TEST(create_resource_with_same_id)
{
	struct wl_display *display;
	struct wl_client *client;
	struct wl_resource *res, *res2;
	int s[2];
	uint32_t id;

	assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0);
	display = wl_display_create();
	assert(display);
	client = wl_client_create(display, s[0]);
	assert(client);

	res = wl_resource_create(client, &wl_display_interface, 2, 0);
	assert(res);
	id = wl_resource_get_id(res);
	assert(wl_client_get_object(client, id) == res);

	/* this one should replace the old one */
	res2 = wl_resource_create(client, &wl_display_interface, 1, id);
	assert(res2 != NULL);
	assert(wl_client_get_object(client, id) == res2);

	wl_resource_destroy(res2);
	wl_resource_destroy(res);

	wl_client_destroy(client);
	wl_display_destroy(display);
	close(s[1]);
}

static void
handle_globals(void *data, struct wl_registry *registry,
	       uint32_t id, const char *intf, uint32_t ver)
{
	struct wl_shm_pool **pool = data;

	if (strcmp(intf, "wl_shm_pool") == 0) {
		(*pool) = wl_registry_bind(registry, id,
					   &wl_shm_pool_interface, ver);
		assert(pool);
	}
}

static const struct wl_registry_listener registry_listener = {
	handle_globals, NULL
};

static void
inert_resource_main(void)
{
	struct client *cli = client_connect();
	struct wl_shm_pool *pool;
	struct wl_registry *reg = wl_display_get_registry(cli->wl_display);
	assert(reg);

	wl_registry_add_listener(reg, &registry_listener, &pool);
	assert(wl_display_roundtrip(cli->wl_display) != -1);
	assert(pool && "Did not bind to the pool");

	wl_registry_destroy(reg);

	/* let the display make the pool resource inert */
	stop_display(cli, 1);
	assert(wl_display_roundtrip(cli->wl_display) != -1);

	/* these requests should be ignored */
	wl_shm_pool_resize(pool, 100);
	wl_shm_pool_resize(pool, 200);

	/* this one should be not */
	wl_shm_pool_destroy(pool);
	assert(wl_display_roundtrip(cli->wl_display) != -1);

	client_disconnect(cli);
}

static void
pool_resize(struct wl_client *client,
	    struct wl_resource *resource,
	    int32_t size)
{
	assert(0 && "This event should be never called");
}

static int destroyed = 0;

static void
pool_destroy(struct wl_client *client,
        struct wl_resource *resource)
{
	destroyed = 1;
}

static const struct wl_shm_pool_interface pool_implementation = {
	.resize = pool_resize,
	.destroy = pool_destroy
};

static void
pool_bind(struct wl_client *client, void *data,
	  uint32_t version, uint32_t id)
{
	struct display *d = data;
	struct client_info *ci;
	struct wl_resource *res;

	wl_list_for_each(ci, &d->clients, link)
		if (ci->wl_client == client)
			break;

	res = wl_resource_create(client, &wl_shm_pool_interface,
				 version, id);
	assert(res);
	wl_resource_set_implementation(res, &pool_implementation, NULL, NULL);

	ci->data = res;
}

TEST(inert_resource)
{
	struct client_info *ci;
	struct wl_resource *res;
	struct display *d = display_create();
	/* we need some interface with destructor. wl_pointer/keyboard
	 * has destructor, but we'd need to implement wl_seat for them too,
	 * so choose rather wl_shm_pool */
	wl_global_create(d->wl_display, &wl_shm_pool_interface,
			 wl_shm_pool_interface.version, d, pool_bind);

	ci = client_create(d, inert_resource_main);
	display_run(d);

	/* display has been stopped, make resource inert */
	res = ci->data;
	assert(res && "Client did not bind to the global");

	assert(!wl_resource_is_inert(res));
	wl_resource_set_inert(res);
	assert(wl_resource_is_inert(res));

	display_resume(d);

	assert(destroyed == 1 && "Destructor was not called");
	display_destroy(d);
}

static void
inert_parent_resource_main(void)
{
	struct client *cli = client_connect();
	struct wl_shm_pool *pool;
	struct wl_registry *reg = wl_display_get_registry(cli->wl_display);
	struct wl_buffer *buffer;
	assert(reg);

	wl_registry_add_listener(reg, &registry_listener, &pool);
	assert(wl_display_roundtrip(cli->wl_display) != -1);
	assert(pool && "Did not bind to the pool");

	wl_registry_destroy(reg);

	/* let the display make the pool resource inert */
	stop_display(cli, 1);
	assert(wl_display_roundtrip(cli->wl_display) != -1);

	/* these requests should be ignored */
	buffer = wl_shm_pool_create_buffer(pool, 0, 100, 100, 4, 0);
	assert(buffer);
	assert(wl_display_roundtrip(cli->wl_display) != -1);

	wl_buffer_destroy(buffer);
	assert(wl_display_roundtrip(cli->wl_display) != -1);

	/* this one should be not */
	wl_shm_pool_destroy(pool);
	assert(wl_display_roundtrip(cli->wl_display) != -1);

	client_disconnect(cli);
}

/* test creating objects on inert object */
TEST(inert_parent_resource)
{
	struct client_info *ci;
	struct wl_resource *res;
	struct display *d = display_create();

	wl_global_create(d->wl_display, &wl_shm_pool_interface,
			 wl_shm_pool_interface.version, d, pool_bind);

	ci = client_create(d, inert_parent_resource_main);
	display_run(d);

	/* display has been stopped, make resource inert */
	res = ci->data;
	assert(res && "Client did not bind to the global");

	assert(!wl_resource_is_inert(res));
	wl_resource_set_inert(res);
	assert(wl_resource_is_inert(res));

	display_resume(d);

	assert(destroyed == 1 && "Destructor was not called");
	display_destroy(d);
}
