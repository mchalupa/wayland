/**
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
#include "wayland-util.h"

#include "tc-events.h"
#include "tc-server.h"
#include "tc-client.h"
#include "tc-utils.h"

/* structure for inner use by eventarray */
struct ea_event {
	struct event event;
	union wl_argument args[MAX_ARGS_NO];

	/* if we are sending the event via socket, we need to know
	 * the arguments' size */
	size_t args_size[MAX_ARGS_NO];

	/* we wouldn't need this one, but it's simpler than go through signature
	 * again (when sending) */
	int args_no;
};

unsigned int
eventarray_add_vl(struct eventarray *ea, enum side side,
		   const struct event *event, va_list vl)
{

	assertf(ea, "eventarray is NULL");
	assert(event);

	/* check if event exist */
	assert(event->interface);
	assertf(event->opcode < (unsigned ) event->interface->event_count,
		"Event opcode is illegal (%d for %s)",
		event->opcode, event->interface->name);

	int index = 0;
	int i = 0;
	const char *tmp;
	struct wl_array *array, *tmp_array;
	struct wl_proxy *proxy;
	struct wl_resource *resource;

	const char *signature
			= event->interface->events[event->opcode].signature;
	assert(signature);

	struct ea_event *e = calloc(1, sizeof *e);
	assert(e && "Out of memory");

	/* copy event */
	e->event = *event;

	/* copy arguments */
	while(signature[i]) {
		assertf(index < MAX_ARGS_NO ,
                        "Too much arguments (wit issue, not wayland)");

		/* index = position in arguments array,
		 *     i = position in signature */
		switch(signature[i]) {
			case 'i':
				e->args[index].i = va_arg(vl, int32_t);
				e->args_size[index] = sizeof(int32_t);
				index++;
				break;
			case 'u':
				e->args[index].u = va_arg(vl, uint32_t);
				e->args_size[index] = sizeof(uint32_t);
				index++;
				break;
			case 'f':
				e->args[index].f = va_arg(vl, wl_fixed_t);
				e->args_size[index] = sizeof(wl_fixed_t);
				index++;
				break;
			case 's':
				tmp = va_arg(vl, const char *);
				assertf(tmp, "No string passed");
				/* this one we'll need to send whole */
				e->args_size[index] = strlen(tmp) + 1;

				e->args[index].s = malloc(e->args_size[index] + 1);
				assert(e->args[index].s && "Out of memory");
				strcpy((char *) e->args[index].s, tmp);

				index++;
				break;
			case 'n':
			case 'o':
				/* save only object's id */
				if (side == CLIENT) {
					proxy = va_arg(vl, struct wl_proxy *);
					e->args[index].n = wl_proxy_get_id(proxy);
				} else {
					resource = va_arg(vl, struct wl_resource *);
					e->args[index].u = wl_resource_get_id(resource);
				}

				e->args_size[index] = sizeof(uint32_t);
				index++;
				break;
			case 'a':
				tmp_array = va_arg(vl, struct wl_array *);
				assertf(tmp_array, "No array passed");

				array = malloc(sizeof(struct wl_array));
				assert(array && "Out of memory");
				wl_array_init(array);

				wl_array_copy(array, tmp_array);
				e->args_size[index] = array->alloc;
				e->args[index].a = array;

				index++;
				break;
			case 'h':
				e->args[index++].h = va_arg(vl, int32_t);
				e->args_size[index] = sizeof(int32_t);
				index++;
				break;
			default:
				break;
		}
		i++;
	}

	/* save how many arguments event has */
	e->args_no = index;

	ea->events[ea->count] = e;
	ea->count++;

	return ea->count;
}

/**
 * Serves the client (or display) to create evenets and then ask the display
 * to emit them when this function is used on display side, side argument has
 * to be set to DISPLAY
 */
unsigned int
eventarray_add(struct eventarray *ea, enum side side,
		   const struct event *event, ...)
{
	va_list vl;
	int stat;

	va_start(vl, event);
	stat = eventarray_add_vl(ea, side, event, vl);
	va_end(vl);

	return stat;
}

/* return pointer to the next type in the signature */
/* (that means skip all non-type parts of signature */
static const char *
get_next_signature(const char *sig)
{
	while (*sig) {
		switch(*sig) {
			case 'i':
			case 'u':
			case 'f':
			case 's':
			case 'n':
			case 'o':
			case 'a':
			case 'h':
				return sig;
			default:
				sig++;
		}
	}

	/* possible terminating zero */
	return sig;
}

static void
convert_ids_to_objects(struct display *d, struct ea_event *e)
{
	int i;
	const char *signature
			= e->event.interface->events[e->event.opcode].signature;

	for(i = 0; i < e->args_no; i++) {
		signature = get_next_signature(signature);
		if (*signature == 'o') {
			e->args[i].o =
				(struct wl_object *) wl_client_get_object(d->wl_client,
									  e->args[i].u);
			assertf(e->args[i].o, "No object like that");
		}

		signature++;
	}
}

static void
convert_objects_to_ids(struct ea_event *e)
{
	int i;
	const char *signature
			= e->event.interface->events[e->event.opcode].signature;

	for(i = 0; i < e->args_no; i++) {
		signature = get_next_signature(signature);
		if (*signature == 'o') {
			e->args[i].u = wl_resource_get_id((void *) e->args[i].o);
			assertf(e->args[i].o, "No object like that");
		}

		signature++;
	}
}

int
eventarray_emit_one(struct display *d, struct eventarray *ea)
{
	assertf(ea->index < ea->count,
		"Index (%d) in eventarray is greater than count (%d)",
		ea->index, ea->count);

	struct wl_resource *resource = NULL;
	struct ea_event *e = ea->events[ea->index];

	/* choose right resource */
	if (e->event.interface == &wl_seat_interface)
		resource = d->resources.wl_seat;
	else if (e->event.interface == &wl_pointer_interface)
		resource = d->resources.wl_pointer;
	else if (e->event.interface ==  &wl_keyboard_interface)
		resource = d->resources.wl_keyboard;
	else if (e->event.interface == &wl_touch_interface)
		resource = d->resources.wl_touch;
	else if (e->event.interface == &wl_surface_interface)
		resource = wl_client_get_object(d->wl_client, e->args[0].u);
	else
		assertf(0, "Unsupported interface");

	assertf(resource, "Resource is not present in the display (%s)",
		e->event.interface->name);


	/* for post_event_array, we need objects, not ids */
	convert_ids_to_objects(d, e);
	wl_resource_post_event_array(resource, e->event.opcode, e->args);
	/* and for later use (comparing etc.) it's good to have id again */
	convert_objects_to_ids(e);
	ea->index++;

	/* return how many events left */
	return ea->count - ea->index;
}

static const char *
event_name_string(struct event *e)
{
	return e->interface->events[e->opcode].name;
}

static char *
print_bytes(void *src, int n)
{
	assert(n < 250 && "Asking for too much bytes");

	char *str = calloc(1, 1000);
	assert(str && "Out of memory");

	int i = 0, printed = 0, cur_pos = 0;
	char *pos = str;

	while (i < n) {
		sprintf(pos + cur_pos, "%#x%n ", *((char *) src + n - i - 1),
			&printed);
		i++;
		cur_pos += printed + 1;

		if (pos - str >= 1000)
			break;
	}

	return str;
}

#define MIN(a,b) (((a)<(b))?(a):(b))
static int
compare_bytes(void *mem1, void *mem2, size_t size1, size_t size2)
{
	assert(mem1);
	assert(mem2);
	assert(size1 > 0);
	assert(size2 > 0);

	int nok = 0;
	char *bytes1, *bytes2;

	if (memcmp(mem1, mem2, MIN(size1, size2) != 0)) {
		bytes1 = print_bytes(mem1, size1);
		bytes2 = print_bytes(mem2, size2);
		assert(bytes1 && bytes2);

		dbg("Different bytes: %s != %s\n"
		    "String: '%*s' != '%*s'\n",
		    bytes1, bytes2,
		    (int) size1, (char *) mem1,
		    (int) size2, (char *) mem2);

		free(bytes1);
		free(bytes2);
		nok = 1;
	}

	/* TODO write extra bytes if size1 != size2 */

	return nok;
}

static int
compare_event_arguments(struct ea_event *e1, struct ea_event *e2, unsigned pos)
{
	int nok = 0;
	int i, printed = 0;
	struct wl_array *a1, *a2;
	const char *sig;

	assert(e1);
	assert(e2);

	sig = e1->event.interface->events[e1->event.opcode].signature;
	assert(sig);

	if (e1->args_no != e2->args_no) {
	      dbg("Different number of arguments (%d and %d)\n",
	      e1->args_no, e2->args_no);
	      nok = 1;
	}

	for (i = 0; i < MIN(e1->args_no, e2->args_no); i++) {
		sig = get_next_signature(sig);

		if (*sig == 'a') {
			a1 = e1->args[i].a;
			a2 = e2->args[i].a;
			if (a1->size != a1->size) {
			      dbg("Sizes of wl_array differs (%lu != %lu)\n",
			      a1->size, a2->size);
			      nok = 1;
			}
			if (a1->alloc != a2->alloc) {
			      dbg("Arrays have different space allocated (%lu != %lu)",
			      a1->alloc, a2->alloc);
			      nok = 1;
			}

			if (compare_bytes(a1->data, a2->data, a1->size, a2->size)) {
				nok = 1;
			}
		} else {
			if (compare_bytes(&e1->args[i], &e2->args[i],
				      e1->args_size[i], e2->args_size[i]))
				nok = 1;
		}

		if (nok && !printed) {
			dbg("Argument %d\n", i);
			printed = 1;
		}

		sig++;
	}

	if (nok)
		dbg("Event on position %u\n", pos);

	return nok;
}

// compare two eventarray and give out description if something differs
int
eventarray_compare(struct eventarray *a, struct eventarray *b)
{
	unsigned n;
	int nok = 0;
	int wrong_count = 0;
	struct ea_event *e1, *e2;

	if (a == b)
		return 0;

	assert(a);
	assert(b);

	if (a->count != b->count) {
		dbg("Different number of events in %s eventarray"
                    "(first %d and second %d)\n",
		    (a->count < b->count) ? "second" : "first", a->count, b->count);

		nok = 1;
		wrong_count = 1;
	}

	for (n = 0; n < MIN(a->count, b->count); n++) {
		e1 = a->events[n];
		e2 = b->events[n];

		if (e1->event.interface != e2->event.interface) {
			dbg("Different interfaces on position %d: (%s and %s)\n",
			n, e1->event.interface->name, e2->event.interface->name);
			nok = 1;
		}
		if (e1->event.opcode != e2->event.opcode) {
			dbg("Different event opcode on position %d: "
			    "have %d (%s->%s) and %d (%s->%s)\n", n,
			    e1->event.opcode, e1->event.interface->name,
			    event_name_string(&e1->event), e2->event.opcode,
			    e2->event.interface->name,
			    event_name_string(&e2->event));
			nok = 1;
		} else if (compare_event_arguments(e1, e2, n) != 0)
			nok = 1;
	}

	// Print info of extra events
	if (nok && wrong_count) {
		if (a->count < b->count) {
			for(; n < b->count; n++)
				dbg("Extra event on position %d (%s->%s)\n",
				    n, b->events[n]->event.interface->name,
				    event_name_string(&b->events[n]->event));
		} else {
			for(; n < a->count; n++)
				dbg("Extra event on position %d (%s->%s)\n",
				    n, a->events[n]->event.interface->name,
				    event_name_string(&a->events[n]->event));
		}
	}

	return nok;
}


/*
 * send event from client side to display
 */
static void
send_event(struct client *c,  struct ea_event *event)
{
	assert(c && event);

	int i, fd;
	void *mem;

	const char *sig
		= event->event.interface->events[event->event.opcode].signature;
	fd = c->sock;

	/* whole event */
	awrite(fd, event, sizeof(struct ea_event));

	/* arguments */
	for (i = 0; i < event->args_no; i++) {
		sig = get_next_signature(sig);

		/* sending string or array */
		if (*sig == 's') {
			mem = (char *) event->args[i].s;
			awrite(fd, mem, event->args_size[i]);
		} else if (*sig == 'a') {
			mem = event->args[i].a->data;
			/* send skeleton */
			awrite(fd, event->args[i].a, sizeof(struct wl_array));
			/* send data */
			awrite(fd, mem, event->args_size[i]);
		} else {
			/* send it as it is */
			awrite(fd, &event->args[i], event->args_size[i]);
		}
		sig++;
	}
}


static struct ea_event *
recieve_event(struct display *d)
{
	int i, fd;
	const char *sig = NULL;

	assert(d);
	fd = d->client_sock[1];

	struct ea_event *e = malloc(sizeof *e);
	assert(e && "Out of memory");

	/* recieve a skeleton */
	aread(fd, e, sizeof(struct ea_event));

	sig = e->event.interface->events[e->event.opcode].signature;

	/* recieve arguments */
	for (i = 0; i < e->args_no; i++) {
		sig = get_next_signature(sig);

		if (*sig == 's') {
			e->args[i].s = malloc(e->args_size[i]);
			assert(e->args[i].s && "Out of memory");

			aread(fd, (char *) &e->args[i].s, e->args_size[i]);
		} else if (*sig == 'a') {
			e->args[i].a = malloc(sizeof(struct wl_array));
			assert(e->args[i].a && "Out of memory");
			aread(fd, e->args[i].a, sizeof(struct wl_array));

			if (e->args_size[i] > 0) {
				e->args[i].a->data = malloc(e->args_size[i]);
				assert(e->args[i].a->data && "Out of memory");
				aread(fd, e->args[i].a->data, e->args_size[i]);
			}
		} else {
			aread(fd, e->args + i, e->args_size[i]);
		}

		sig++;
	}

	return e;
}


void
eventarray_send(struct client *c, struct eventarray *ea)
{
	assert(c);
	assert(ea);

	unsigned i;
	awrite(c->sock, ea, sizeof(struct eventarray));

	for (i = 0; i < ea->count; i++) {
		send_event(c, ea->events[i]);
	}
}


struct eventarray *
eventarray_recieve(struct display *d)
{
	assert(d);

	unsigned int i;

	struct eventarray *ea = malloc(sizeof(struct eventarray));
	assert(ea && "Out of memory");

	/* skeleton */
	aread(d->client_sock[1], ea, sizeof(struct eventarray));

	for (i = 0; i < ea->count; i++) {
		ea->events[i] = recieve_event(d);
	}

	return ea;
}


static void
free_event_args(struct ea_event *e)
{
	assert(e);

	int i;
	const char *sig =
		e->event.interface->events[e->event.opcode].signature;
	assert(sig);

	for (i = 0; i < e->args_no; i++) {
		sig = get_next_signature(sig);

		if (*sig == 's') {
			free((void *) e->args[i].s);
		} else if (*sig == 'a') {
			wl_array_release(e->args[i].a);
			free(e->args[i].a);
		}

		sig++;
	}
}


void
eventarray_free(struct eventarray *ea)
{
	unsigned i;

	assert(ea);

	for (i = 0; i < ea->count; i++) {
		free_event_args(ea->events[i]);
		free(ea->events[i]);
	}

	free(ea);
}

struct eventarray *
eventarray_create()
{
	struct eventarray *ea = calloc(1, sizeof(struct eventarray));
	assert(ea && "Out of memory");

	return ea;
}
