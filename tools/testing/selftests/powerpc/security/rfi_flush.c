// SPDX-License-Identifier: GPL-2.0+

/*
 * Copyright 2018 IBM Corporation.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#define __SANE_USERSPACE_TYPES__

#include <sys/types.h>
#include <stdint.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils.h"

#define CACHELINE_SIZE 128

struct perf_event_read {
	__u64 nr;
	__u64 l1d_misses;
};

static inline __u64 load(void *addr)
{
	__u64 tmp;

	asm volatile("ld %0,0(%1)" : "=r"(tmp) : "b"(addr));

	return tmp;
}

static void syscall_loop(char *p, unsigned long iterations,
			 unsigned long zero_size)
{
	for (unsigned long i = 0; i < iterations; i++) {
		for (unsigned long j = 0; j < zero_size; j += CACHELINE_SIZE)
			load(p + j);
		getppid();
	}
}

int rfi_flush_test(void)
{
	char *p;
	int repetitions = 10;
	int fd, passes = 0, iter, rc = 0;
	struct perf_event_read v;
	__u64 l1d_misses_total = 0;
	unsigned long iterations = 100000, zero_size = 24 * 1024;
	unsigned long l1d_misses_expected;
	int rfi_flush_orig, rfi_flush;
	int have_entry_flush, entry_flush_orig;

	SKIP_IF(geteuid() != 0);

	if (read_debugfs_file("powerpc/rfi_flush", &rfi_flush_orig) < 0) {
		perror("Unable to read powerpc/rfi_flush debugfs file");
		SKIP_IF(1);
	}

	if (read_debugfs_file("powerpc/entry_flush", &entry_flush_orig) < 0) {
		have_entry_flush = 0;
	} else {
		have_entry_flush = 1;

		if (entry_flush_orig != 0) {
			if (write_debugfs_file("powerpc/entry_flush", 0) < 0) {
				perror("error writing to powerpc/entry_flush debugfs file");
				return 1;
			}
		}
	}

	rfi_flush = rfi_flush_orig;

	fd = perf_event_open_counter(PERF_TYPE_RAW, /* L1d miss */ 0x400f0, -1);
	FAIL_IF(fd < 0);

	p = (char *)memalign(zero_size, CACHELINE_SIZE);

	FAIL_IF(perf_event_enable(fd));

	// disable L1 prefetching
	set_dscr(1);

	iter = repetitions;

	/*
	 * We expect to see l1d miss for each cacheline access when rfi_flush
	 * is set. Allow a small variation on this.
	 */
	l1d_misses_expected = iterations * (zero_size / CACHELINE_SIZE - 2);

again:
	FAIL_IF(perf_event_reset(fd));

	syscall_loop(p, iterations, zero_size);

	FAIL_IF(read(fd, &v, sizeof(v)) != sizeof(v));

	if (rfi_flush && v.l1d_misses >= l1d_misses_expected)
		passes++;
	else if (!rfi_flush && v.l1d_misses < (l1d_misses_expected / 2))
		passes++;

	l1d_misses_total += v.l1d_misses;

	while (--iter)
		goto again;

	if (passes < repetitions) {
		printf("FAIL (L1D misses with rfi_flush=%d: %llu %c %lu) [%d/%d failures]\n",
		       rfi_flush, l1d_misses_total, rfi_flush ? '<' : '>',
		       rfi_flush ? repetitions * l1d_misses_expected :
		       repetitions * l1d_misses_expected / 2,
		       repetitions - passes, repetitions);
		rc = 1;
	} else
		printf("PASS (L1D misses with rfi_flush=%d: %llu %c %lu) [%d/%d pass]\n",
		       rfi_flush, l1d_misses_total, rfi_flush ? '>' : '<',
		       rfi_flush ? repetitions * l1d_misses_expected :
		       repetitions * l1d_misses_expected / 2,
		       passes, repetitions);

	if (rfi_flush == rfi_flush_orig) {
		rfi_flush = !rfi_flush_orig;
		if (write_debugfs_file("powerpc/rfi_flush", rfi_flush) < 0) {
			perror("error writing to powerpc/rfi_flush debugfs file");
			return 1;
		}
		iter = repetitions;
		l1d_misses_total = 0;
		passes = 0;
		goto again;
	}

	perf_event_disable(fd);
	close(fd);

	set_dscr(0);

	if (write_debugfs_file("powerpc/rfi_flush", rfi_flush_orig) < 0) {
		perror("unable to restore original value of powerpc/rfi_flush debugfs file");
		return 1;
	}

	if (have_entry_flush) {
		if (write_debugfs_file("powerpc/entry_flush", entry_flush_orig) < 0) {
			perror("unable to restore original value of powerpc/entry_flush "
			       "debugfs file");
			return 1;
		}
	}

	return rc;
}

int main(int argc, char *argv[])
{
	return test_harness(rfi_flush_test, "rfi_flush_test");
}
