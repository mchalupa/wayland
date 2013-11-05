#ifndef __TC_EVENTS_H__
#define __TC_EVENTS_H__

#include "wayland-util.h"
#include "tc-utils.h"

struct display;
struct client;

#define MAX_ARGS_NO 15
#define MAX_EVENTS 100

/**
 * Usage:
 *  EVENT_DEFINE(send_motion, &wl_pointer_interface, WL_POINTER_MOTION);
 *
 *  eventarray_add(send_motion, arg1, arg2, arg3, arg4);
 *  eventarray_add(send_motion, 1, 2, 3, 4);
 *
 *  ...
 *  ...
 */

struct event {
	const struct wl_interface *interface;
	uint32_t opcode;
};

struct ea_event;
struct eventarray {
	struct ea_event *events[MAX_EVENTS];

	unsigned count;
	unsigned index;
};

/* we use pointer in all functions, so create event as opaque structure
 * and "return" pointer */
#define EVENT_DEFINE(eventname, intf, opcode) 					\
	assertf((opcode) < ((struct wl_interface *) (intf))->event_count,	\
		"EVENT_DEFINE: Event opcode is illegal (%d for '%s')",		\
		(opcode),((struct wl_interface *) (intf))->name);		\
	static const struct event __event_##eventname##_			\
							= {(intf), (opcode)};	\
	static const struct event *(eventname) = &__event_##eventname##_;

/* if we won't use assertf, we can do it globaly */
#define EVENT_DEFINE_GLOBAL(eventname, intf, opcode)				\
	static const struct event __event_##eventname##_			\
							= {(intf), (opcode)};	\
	static const struct event *(eventname) = &__event_##eventname##_;

struct eventarray *
eventarray_create();

/*
 * side = {CLIENT|DISPLAY}
 */
unsigned int
eventarray_add(struct eventarray *ea, enum side side,
			const struct event *event, ...);

unsigned int
eventarray_add_vl(struct eventarray *ea, enum side side,
		   const struct event *event, va_list vl);

int
eventarray_emit_one(struct display *d, struct eventarray *ea);

int
eventarray_compare(struct eventarray *a, struct eventarray *b);

struct eventarray *
eventarray_recieve(struct display *d);

void
eventarray_send(struct client *c, struct eventarray *ea);

void
eventarray_free(struct eventarray *ea);
#endif /* __TC_EVENTS_H__ */
