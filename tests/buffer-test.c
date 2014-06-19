/*
 * Copyright (c) 2014 Red Hat, Inc.
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "test-runner.h"
#include "wayland-private.h"
#include <sys/uio.h>
#include <unistd.h>

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


static const char data[] = "abcdefghijklmnopqrstuvwxyz";

static int
fill_buffer(struct wl_buffer *b)
{
	int i;
	for (i = 1; i * (sizeof data) <= sizeof b->data; i++) {
		wl_buffer_put(b, data, sizeof data);

		assert(b->tail == 0);
		assertf(b->head == i * (sizeof data),
			"head is %u instead of %lu", b->head, i * (sizeof data));
		assertf(wl_buffer_size(b) == i * sizeof data,
			"wl_buffer_size() = %u instead of %lu",
			wl_buffer_size(b), i * sizeof data);
	}

	return i;
}

TEST(wl_buffer_put_tst)
{
	struct wl_buffer b = {.head = 0, .tail = 0};
	size_t index;
	int i;

	assert(wl_buffer_size(&b) == 0);

	i = fill_buffer(&b);

	/* do overflow */
	wl_buffer_put(&b, data, sizeof data);

	/* check for me */
	assert(i * sizeof data > sizeof b.data);

	/* size should not be cropped */
	assertf(wl_buffer_size(&b) == i * sizeof data,
		"head: %u, tail: %u, wl_buffer_size: %u",
		b.head, b.tail, wl_buffer_size(&b));

	/* head must be somewhere on the begining of the buffer after owerflow */
	assertf(MASK(b.head) < sizeof b.data);
	assert(b.tail == 0);

	/* get begining of the last string written to buffer */
	index = sizeof b.data % sizeof data;

	/* compare if the string is split (after overflow) right */
	assertf(strncmp((char *) b.data + sizeof b.data - index, data, index - 1) == 0,
		"Should have '%*s', but have '%*s'\n", (int) (index - 1), data,
		(int) (index - 1), (char *) b.data + sizeof b.data - index);
	assertf(strncmp(b.data, (char *) data + index, sizeof data - index) == 0,
		"Should have '%*s', but have '%*s'\n",
		(int) (sizeof data - index), data + index,
		(int) (sizeof data - index), (char *) b.data);

	struct wl_buffer bb = {.head = 0, .tail = 0};
	wl_buffer_put(&bb, data, sizeof data);
	assert(strcmp(data, bb.data) == 0);
}

TEST(wl_buffer_fill_alot)
{
	struct wl_buffer b = {.head = 0, .tail = 0};
	int i;

	/* put a lot of data into buffer (data < sizeof buffer.data) */
	for (i = 0; i * sizeof data < 100 * sizeof b.data; i++)
		wl_buffer_put(&b, data, sizeof data);
}

TEST(wl_buffer_copy_tst)
{
	char buf[40];
	struct wl_buffer b = {.head = 0, .tail = 0};

	wl_buffer_put(&b, data, sizeof data);
	wl_buffer_copy(&b, &buf, sizeof data);

	assert(strcmp(buf, b.data) == 0);

	/* wl_buffer_copy is not destructive */
	assertf(strcmp(buf, b.data) == 0,
		"Previous wl_buffer_copy modified data");
	assert(b.tail == 0);

	/* do overflow */
	b.head = sizeof b.data - 10;
	b.tail = b.head;
	wl_buffer_put(&b, data, sizeof data);

	memset(&buf, 0, sizeof buf);
	wl_buffer_copy(&b, buf, sizeof data);
	assert(strcmp(buf, data) == 0);
}

TEST(wl_buffer_put_iov_tst)
{
	struct wl_buffer b = {.head = 0, .tail = 0};
	struct iovec iov[2];
	int cnt = 0;
	size_t len;

	int p[2];
	char buf1[] = "buffer1";
	char buf2[] = "buffer2";

	assert(pipe(p) != -1);

	/* try empty buffer */
	wl_buffer_put_iov(&b, iov, &cnt);
	assertf(cnt == 1, "Count: %d", cnt);

	write(p[1], buf1, sizeof buf1);
	len = readv(p[0], iov, cnt);
	assertf(len == sizeof buf1, "len: %lu, sizeof buf1: %lu",
		len, sizeof buf1);
	assert(strcmp(buf1, b.data) == 0);

	b.head += len;
	wl_buffer_put_iov(&b, iov, &cnt);
	assertf(cnt == 1, "Count: %d", cnt);

	write(p[1], buf2, sizeof buf2);
	len = readv(p[0], iov, cnt);
	assertf(len == sizeof buf2, "len: %lu, sizeof buf2: %lu",
		len, sizeof buf2);
	/* the contents should be "buffer1buffer2" */
	assert(strcmp(b.data, buf1) == 0);
	assert(strcmp(b.data + sizeof buf1, buf2) == 0);

	/* set head 3 bytes from the end --> it should write only 3 bytes */
	b.head = sizeof b.data - 3;
	wl_buffer_put_iov(&b, iov, &cnt);
	assertf(cnt == 1, "Count: %d", cnt);
	write(p[1], buf1, sizeof buf1);
	len = readv(p[0], iov, cnt);
	assertf(len == 3, "len: %lu", len);

	/* set tail != 0, it should fill both iovec structures */
	b.tail = 5;
	wl_buffer_put_iov(&b, iov, &cnt);
	assertf(cnt == 2, "Count: %d", cnt);
	/* in the pipe left 5 bytes {'f', 'e', 'r', '1', '\0'}*/
	len = readv(p[0], iov, cnt);
	assertf(len == 5, "len: %lu", len);
	assert(strncmp(b.data + sizeof b.data - 3, "fer", 3) == 0);
	assert(strcmp(b.data, "1") == 0);

	close(p[0]);
	close(p[1]);
}

TEST(wl_buffer_get_iov_tst)
{
	struct wl_buffer b = {.head = 0, .tail = 0};
	struct wl_buffer tmp;
	struct iovec iov[2];
	int cnt = 0;
	size_t index;
	ssize_t len;

	int p[2];

	assert(pipe(p) != -1);

	fill_buffer(&b);
	index = sizeof b.data % sizeof data;

	wl_buffer_get_iov(&b, iov, &cnt);
	assert((len = writev(p[1], iov, cnt)) > 0);
	assert(read(p[0], &tmp.data, sizeof b.data - index) == len);
	assert(strcmp(tmp.data, b.data) == 0);

	/* circulation */
	b.tail = sizeof b.data - 10;
	b.head = b.tail;
	wl_buffer_put(&b, data, sizeof data);

	wl_buffer_get_iov(&b, iov, &cnt);
	assertf(cnt == 2, "cnt: %d", cnt);
	assert((len = writev(p[1], iov, cnt)) > 0);
	assert(read(p[0], &tmp.data, sizeof data) == len);
	assert(strncmp(tmp.data, b.data + sizeof b.data - 10, 10) == 0);
	assert(strcmp(tmp.data + 10, b.data) == 0);

	close(p[0]);
	close(p[1]);
}

TEST(wl_buffer_get_put_iov_tst)
{
	struct wl_buffer b1, b2;
	struct iovec iov1[2], iov2[2];
	int cnt1 = 0;
	int cnt2 = 0;

	int p[2];

	assert(pipe(p) != -1);
	memset(&b1, 0, sizeof b1);
	memset(&b2, 0, sizeof b2);

	fill_buffer(&b1);
	wl_buffer_get_iov(&b1, iov1, &cnt1);
	wl_buffer_put_iov(&b2, iov2, &cnt2);
	assert((writev(p[1], iov1, cnt1) == readv(p[0], iov2, cnt2)) > 0);
	assert(memcmp(b1.data, b2.data, sizeof b1.data) == 0);

	/* try cycled buffer (head < tail) */
	b1.head = 10;
	b1.tail = sizeof b1.data - 10;
	b2.head = b1.tail;
	b2.tail = b1.head;
	wl_buffer_get_iov(&b1, iov1, &cnt1);
	wl_buffer_put_iov(&b2, iov2, &cnt2);
	assertf(cnt1 == 2 && cnt2 == 2, "cnt1: %d, cnt2: %d", cnt1, cnt2);
	assert((writev(p[1], iov1, cnt1) == readv(p[0], iov2, cnt2)) > 0);
	assert(memcmp(b1.data, b2.data, sizeof b1.data) == 0);

	close(p[0]);
	close(p[1]);
}
