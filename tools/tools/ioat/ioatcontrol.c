/*-
 * Copyright (C) 2012 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/ioctl.h>
#include <sys/queue.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <libutil.h>

#include "ioat_test.h"

static int prettyprint(struct ioat_test *);

static void
usage(void)
{

	printf("Usage: %s [-fV] <channel #> <txns> [<bufsize> "
	    "[<chain-len> [duration]]]\n", getprogname());
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	struct ioat_test t;
	int fd, ch;
	bool fflag;

	while ((ch = getopt(argc, argv, "fV")) != -1) {
		switch (ch) {
		case 'f':
			fflag = true;
			break;
		case 'V':
			t.verify = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	/* Defaults for optional args */
	t.buffer_size = 256 * 1024;
	t.chain_depth = 2;
	t.duration = 0;
	t.testkind = IOAT_TEST_DMA;

	if (fflag)
		t.testkind = IOAT_TEST_FILL;

	t.channel_index = atoi(argv[0]);
	if (t.channel_index > 8) {
		printf("Channel number must be between 0 and 7.\n");
		return (EX_USAGE);
	}

	t.transactions = atoi(argv[1]);

	if (argc >= 3) {
		t.buffer_size = atoi(argv[2]);
		if (t.buffer_size == 0) {
			printf("Buffer size must be greater than zero\n");
			return (EX_USAGE);
		}
	}

	if (argc >= 4) {
		t.chain_depth = atoi(argv[3]);
		if (t.chain_depth < 1) {
			printf("Chain length must be greater than zero\n");
			return (EX_USAGE);
		}
	}

	if (argc >= 5) {
		t.duration = atoi(argv[4]);
		if (t.duration < 1) {
			printf("Duration must be greater than zero\n");
			return (EX_USAGE);
		}
	}

	fd = open("/dev/ioat_test", O_RDWR);
	if (fd < 0) {
		printf("Cannot open /dev/ioat_test\n");
		return (EX_UNAVAILABLE);
	}

	(void)ioctl(fd, IOAT_DMATEST, &t);
	close(fd);

	return (prettyprint(&t));
}

static int
prettyprint(struct ioat_test *t)
{
	char bps[10], bytesh[10];
	uintmax_t bytes;

	if (t->status[IOAT_TEST_NO_DMA_ENGINE] != 0 ||
	    t->status[IOAT_TEST_NO_MEMORY] != 0 ||
	    t->status[IOAT_TEST_MISCOMPARE] != 0) {
		printf("Errors:\n");
		if (t->status[IOAT_TEST_NO_DMA_ENGINE] != 0)
			printf("\tNo DMA engine present: %u\n",
			    (unsigned)t->status[IOAT_TEST_NO_DMA_ENGINE]);
		if (t->status[IOAT_TEST_NO_MEMORY] != 0)
			printf("\tOut of memory: %u\n",
			    (unsigned)t->status[IOAT_TEST_NO_MEMORY]);
		if (t->status[IOAT_TEST_MISCOMPARE] != 0)
			printf("\tMiscompares: %u\n",
			    (unsigned)t->status[IOAT_TEST_MISCOMPARE]);
	}

	printf("Processed %u txns\n", (unsigned)t->status[IOAT_TEST_OK] /
	    t->chain_depth);
	bytes = (uintmax_t)t->buffer_size * t->status[IOAT_TEST_OK];

	humanize_number(bytesh, sizeof(bytesh), (int64_t)bytes, "B",
	    HN_AUTOSCALE, HN_DECIMAL);
	if (t->duration) {
		humanize_number(bps, sizeof(bps),
		    (int64_t)1000 * bytes / t->duration, "B/s", HN_AUTOSCALE,
		    HN_DECIMAL);
		printf("%ju (%s) copied in %u ms (%s)\n", bytes, bytesh,
		    (unsigned)t->duration, bps);
	} else
		printf("%ju (%s) copied\n", bytes, bytesh);

	return (EX_OK);
}
