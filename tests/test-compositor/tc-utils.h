#ifndef __TC_UTILS_H__
#define __TC_UTILS_H__

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Definitions visible for both - server and client
 */

enum optype {
	/* arguments: int can (0 or 1)*/
	CAN_CONTINUE = 1,	/* client can continue */

	/* arguments: int count*/
	EVENT_COUNT,	   	/* how many events can display emit */

	/* arguments: struct wit_event, union wl_argument *args */
	EVENT_EMIT,		/* ask for single event */

	/* arguments: none */
	RUN_FUNC,		/* run user's func */

	/* arguments: size_t size, void *mem */
	SEND_BYTES,		/* send raw bytestream to the other side */

	/* arguments: unsigned count */
	SEND_EVENTARRAY,	/* send eventarray, used only for acknowledge */

	/* arguments: none */
	BARRIER,		/* sync client with display */
};

/* can be used to define which side is process on */
enum side {
	CLIENT = 0,
	DISPLAY = 1
};

/* hide warrnings about wl_buffer */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <wayland-client-protocol.h>

const struct wl_registry_listener registry_default_listener;

/* write with assert check */
int
awrite(int fd, void *src, size_t size);

int
aread(int fd, void *dest, size_t size);

/* send operation via socket */
void
send_message(int fd, enum optype op, ...);


/* Assert with formated output */
#define assertf(cond, ...) 							\
	do {									\
		if (!(cond)) {							\
			fprintf(stderr, "%s (%s: %d): Assertion %s failed!",	\
					__FUNCTION__, __FILE__, __LINE__, #cond);\
			fprintf(stderr, " " __VA_ARGS__);			\
			putc('\n', stderr);					\
			abort();						\
		}								\
	} while (0)

/* Print debug message */
#define dbg(...) 								\
	do {									\
		fprintf(stderr, "[%d | %s in %s: %d] ", getpid(),		\
				__FUNCTION__, __FILE__, __LINE__);		\
		fprintf(stderr,	__VA_ARGS__);					\
	} while (0)

/* Print debug message if the condition cond is true */
#define ifdbg(cond, ...)			\
	do {					\
		if (cond)			\
			dbg(__VA_ARGS__);	\
	} while (0)

#endif  /* __TC_UTILS_H__ */
