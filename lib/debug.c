// SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
// SPDX-License-Identifier: Apache-2.0
#ifndef __KERNEL__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#endif /* __KERNEL__ */

#include "config.h"
#include "debug.h"

void mvrlu_dump_stack(void)
{
#ifndef __KERNEL__
	/*
	 * quick and dirty backtrace implementation
	 * - http://stackoverflow.com/questions/4636456/how-to-get-a-stack-trace-for-c-using-gcc-with-line-number-information
	 */
	char pid_buf[30];
	char name_buf[512];
	int child_pid;

	sprintf(pid_buf, "%d", getpid());
	name_buf[readlink("/proc/self/exe", name_buf, 511)] = 0;
	child_pid = fork();

	if (!child_pid) {
		dup2(2, 1); /* redirect output to stderr */
		fprintf(stdout, "stack trace for %s pid=%s\n", name_buf,
			pid_buf);
		execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex",
		       "bt", name_buf, pid_buf, NULL);
		fprintf(stdout, "gdb is not installed. ");
		fprintf(stdout, "Please, install gdb to see stack trace.");
		abort(); /* If gdb failed to start */
	} else
		waitpid(child_pid, NULL, 0);
#else /* __KERNEL__ */
	dump_stack();
#endif /* __KERNEL__ */
}

void mvrlu_attach_gdb(void)
{
#ifndef __KERNEL__
	char pid_buf[30];
	char name_buf[512];
	int child_pid;

	sprintf(pid_buf, "%d", getpid());
	name_buf[readlink("/proc/self/exe", name_buf, 511)] = 0;
	child_pid = fork();

	if (!child_pid) {
		dup2(2, 1); /* redirect output to stderr */
		fprintf(stdout, "stack trace for %s pid=%s\n", name_buf,
			pid_buf);
		execlp("gdb", "gdb", name_buf, pid_buf, NULL);
		fprintf(stdout, "gdb is not installed. ");
		fprintf(stdout, "Please, install gdb to see stack trace.");
		abort(); /* If gdb failed to start */
	} else
		waitpid(child_pid, NULL, 0);
#endif /* __KERNEL__ */
}

void mvrlu_assert_fail(void)
{
#ifdef __KERNEL__
	BUG();
	/* if that doesn't kill us, halt */
	panic("Oops failed to kill thread");
#else
	fflush(stdout);
	fflush(stderr);
	mvrlu_dump_stack();
#if MVRLU_ATTACH_GDB_ASSERT_FAIL
	rlu_attach_gdb();
#endif /* MVRLU_ATTACH_GDB_ASSERT_FAIL */
	abort();
#endif /* __KERNEL__ */
}
