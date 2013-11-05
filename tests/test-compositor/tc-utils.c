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

#include <unistd.h>

#include "tc-utils.h"

int
awrite(int fd, void *src, size_t size)
{
	size_t stat = write(fd, src, size);
	assertf(stat == size,
		"Sent %lu instead of %lu bytes",	stat, size);

	return stat;
}

int
aread(int fd, void *dest, size_t size)
{
	size_t stat = read(fd, dest, size);
	assertf(stat == size,
		"Recieved %lu instead of %lu bytes", stat, size);

	return stat;
}

/* Send messages to counterpart */
void
send_message(int fd, enum optype op, ...)
{
	va_list vl;
	int cont, count;
	void *mem;
	size_t size;

	/* enum optype is defined from 1 */
	assertf(op > 0, "Wrong operation");
	assertf(fd >= 0, "Wrong filedescriptor");

	va_start(vl, op);

	switch (op) {
		case CAN_CONTINUE:
			cont = va_arg(vl, int);
			assertf(cont == 0 || cont == 1,
				"CAN_CONTINUE argument can be either 0 or 1"
				" (is %d)", cont);

			awrite(fd, &op, sizeof(op));
			awrite(fd, &cont, sizeof(int));
			break;
		case BARRIER:
		case RUN_FUNC:
		case EVENT_EMIT:
			/* used only to kick and acknowledge */
			awrite(fd, &op, sizeof(op));
			break;
		case EVENT_COUNT:
			count = va_arg(vl, int);
			assertf(count >= 0,
				"EVENT_COUNT argument must be positive (%d)",
				count);

			awrite(fd, &op, sizeof(op));
			awrite(fd, &count, sizeof(int));
			break;
		case SEND_BYTES:
			mem = va_arg(vl, void *);
			assertf(mem, "SEND_BYTES: Passed NULL data");
			size = va_arg(vl, size_t);
			assertf(size > 0, "SEND_BYTES: size must be greater than 0 (%lu)",
				size);

			awrite(fd, &op, sizeof(op));
			awrite(fd, &size, sizeof(size_t));
			awrite(fd, mem, size);
			break;
		case SEND_EVENTARRAY:
			assertf(0, "Use wit_display_recieve_eventarray() "
				"and wit_client_send_eventarray() instead");
			break;
		default:
			assertf(0, "Unknown operation (%d)", op);
	}

	va_end(vl);
}
