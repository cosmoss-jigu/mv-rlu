// SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
// SPDX-License-Identifier: Apache-2.0
#ifndef _DEBUG_H
#define _DEBUG_H

#include "arch.h"
#include "mvrlu_i.h"

//#define MVRLU_ENABLE_ASSERT
//#define MVRLU_ENABLE_FREE_POISIONING
#define MVRLU_ENABLE_STATS
//#define MVRLU_TIME_MEASUREMENT
#define MVRLU_ATTACH_GDB_ASSERT_FAIL                                           \
	0 /* attach gdb at MVRLU_ASSERT() failure */

#ifdef __KERNEL__
#undef MVRLU_TIME_MEASUREMENT
#undef MVRLU_ENABLE_STATS
#endif

#define MVRLU_FREE_POSION ((unsigned char)(0xbd))

#define MVRLU_COLOR_RED "\x1b[31m"
#define MVRLU_COLOR_GREEN "\x1b[32m"
#define MVRLU_COLOR_YELLOW "\x1b[33m"
#define MVRLU_COLOR_BLUE "\x1b[34m"
#define MVRLU_COLOR_MAGENTA "\x1b[35m"
#define MVRLU_COLOR_CYAN "\x1b[36m"
#define MVRLU_COLOR_RESET "\x1b[0m"

#ifdef MVRLU_TIME_MEASUREMENT
#define mvrlu_start_timer()                                                    \
	{                                                                      \
		unsigned long __b_e_g_i_n__;                                   \
		__b_e_g_i_n__ = read_tsc()
#define mvrlu_stop_timer(tick)                                                 \
	(tick) += read_tsc() - __b_e_g_i_n__;                                  \
	}
#else
#define mvrlu_start_timer()
#define mvrlu_stop_timer(tick)
#endif /* MVRLU_TIME_MEASUREMENT */

#define mvrlu_trace_global(fmt, ...)                                           \
	fprintf(stderr, "\n%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__,    \
		##__VA_ARGS__);

#define mvrlu_trace(self, fmt, ...)                                            \
	fprintf(stderr, "\n[%d][%d][%ld]:%s:%d:%s(): " fmt, self->thread_id,   \
		self->run_counter, self->local_version, __FILE__, __LINE__,    \
		__func__, ##__VA_ARGS__);

#ifdef MVRLU_ENABLE_ASSERT
#define mvrlu_assert(cond)                                                     \
	if (unlikely(!(cond))) {                                               \
		extern void mvrlu_assert_fail(void);                           \
		printf("\n-----------------------------------------------\n"); \
		printf("\nAssertion failure: %s:%d '%s'\n", __FILE__,          \
		       __LINE__, #cond);                                       \
		mvrlu_assert_fail();                                           \
	}

#define mvrlu_assert_msg(cond, self, fmt, ...)                                 \
	if (unlikely(!(cond))) {                                               \
		extern void mvrlu_assert_fail(void);                           \
		printf("\n-----------------------------------------------\n"); \
		printf("\nAssertion failure: %s:%d '%s'\n", __FILE__,          \
		       __LINE__, #cond);                                       \
		MVRLU_TRACE(self, fmt, ##__VA_ARGS__);                         \
		mvrlu_assert_fail();                                           \
	}
#else
#define mvrlu_assert(cond)
#define mvrlu_assert_msg(cond, self, fmt, ...)
#endif /* MVRLU_ENABLE_ASSERT */

#define mvrlu_bug()                                                            \
	do {                                                                   \
		int *poison = NULL;                                            \
		*poison = 0xdeaddead;                                          \
	} while (0)

#define mvrlu_panic(cond)                                                      \
	if (unlikely(!(cond))) {                                               \
		extern void mvrlu_assert_fail(void);                           \
		printf("\n-----------------------------------------------\n"); \
		printf("\nPanic!: %s:%d '%s'\n", __FILE__, __LINE__, #cond);   \
		mvrlu_assert_fail();                                           \
	}

#define mvrlu_panic_msg(cond, self, fmt, ...)                                  \
	if (unlikely(!(cond))) {                                               \
		extern void mvrlu_assert_fail(void);                           \
		printf("\n-----------------------------------------------\n"); \
		printf("\nPanic!: %s:%d '%s'\n", __FILE__, __LINE__, #cond);   \
		MVRLU_TRACE(self, fmt, ##__VA_ARGS__);                         \
		mvrlu_assert_fail();                                           \
	}

#define mvrlu_warning(cond) mvrlu_assert(cond)
#define mvrlu_warning_msg(cond, self, fmt, ...)                                \
	mvrlu_assert_msg(cond, self, fmt, ##__VA_ARGS__)

#ifdef MVRLU_ENABLE_TRACE_0
#define trace_0(self, fmt, ...) mvrlu_trace(self, fmt, ##__VA_ARGS__)
#define trace_0_global(fmt, ...) mvrlu_trace_global(fmt, ##__VA_ARGS__)
#else
#define trace_0(self, fmt, ...)
#define trace_0_global(self, fmt, ...)
#endif

#ifdef MVRLU_ENABLE_TRACE_1
#define trace_1(self, fmt, ...) mvrlu_trace(self, fmt, ##__VA_ARGS__)
#define trace_1_global(fmt, ...) mvrlu_trace_global(fmt, ##__VA_ARGS__)
#else
#define trace_1(self, fmt, ...)
#define trace_1_global(fmt, ...)
#endif

#ifdef MVRLU_ENABLE_TRACE_2
#define trace_2(self, fmt, ...) mvrlu_trace(self, fmt, ##__VA_ARGS__)
#define trace_2_global(fmt, ...) mvrlu_trace_global(fmt, ##__VA_ARGS__)
#else
#define trace_2(self, fmt, ...)
#define trace_2_global(fmt, ...)
#endif

#ifdef MVRLU_ENABLE_TRACE_3
#define trace_3(self, fmt, ...) mvrlu_trace(self, fmt, ##__VA_ARGS__)
#define trace_3_global(fmt, ...) mvrlu_trace_global(fmt, ##__VA_ARGS__)
#else
#define trace_3(self, fmt, ...)
#define trace_3_global(fmt, ...)
#endif
#endif /* _DEBUG_H */
