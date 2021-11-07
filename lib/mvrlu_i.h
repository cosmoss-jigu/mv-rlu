// SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
// SPDX-License-Identifier: Apache-2.0
#ifndef _MVRLU_I_H
#define _MVRLU_I_H
#include "arch.h"
#include "config.h"
#include "debug.h"

#define MAX_VERSION (ULONG_MAX - 1)
#define MIN_VERSION (0ul)

#define STAT_NAMES                                                             \
	S(n_starts)                                                            \
	S(n_finish)                                                            \
	S(n_aborts)                                                            \
	S(n_low_mark_wakeup)                                                   \
	S(n_high_mark_block)                                                   \
	S(max_log_used_bytes)                                                  \
	S(n_reclaim)                                                           \
	S(n_reclaim_wrt_set)                                                   \
	S(n_reclaim_copy)                                                      \
	S(n_reclaim_free)                                                      \
	S(n_qp_detect)                                                         \
	S(n_qp_nap)                                                            \
	S(n_qp_help_reclaim)                                                   \
	S(n_qp_zombie_reclaim)                                                 \
	S(max__)
#define S(x) stat_##x,

enum { STAT_NAMES };

enum { TYPE_ACTUAL = 0, /* actual object */
       TYPE_WRT_SET, /* log: write set header */
       TYPE_COPY, /* log: copied version */
       TYPE_FREE, /* log: copy whose actual object is requested to free */
       TYPE_BOGUS, /* log: bogus to skip the end of a log */
};

enum { THREAD_LIVE = 0, /* live thread */
       THREAD_LIVE_ZOMBIE, /* finished but not-yet-recalimed thread */
       THREAD_DEAD_ZOMBIE, /* zombie thread that is requested to be reclaimed */
};

typedef struct mvrlu_stat {
	unsigned long cnt[stat_max__];
} mvrlu_stat_t;

typedef struct mvrlu_obj_hdr {
	volatile unsigned int obj_size; /* object size for copy */
	volatile unsigned short padding_size; /* passing size in log */
	volatile unsigned short type;
	volatile void *p_copy;
	unsigned char obj[0]; /* start address of a read object */
} ____ptr_aligned mvrlu_obj_hdr_t;

typedef struct mvrlu_act_hdr {
	volatile void *p_lock;
} ____ptr_aligned mvrlu_act_hdr_t;

typedef struct mvrlu_cpy_hdr {
	volatile unsigned long *p_wrt_clk;
	volatile unsigned long wrt_clk_next;
	volatile unsigned long __wrt_clk;
	volatile void *p_act;
} ____ptr_aligned mvrlu_cpy_hdr_t;

typedef struct mvrlu_act_hdr_struct {
	mvrlu_act_hdr_t act_hdr;
	mvrlu_obj_hdr_t obj_hdr;
} __packed mvrlu_act_hdr_struct_t;

typedef struct mvrlu_cpy_hdr_struct {
	mvrlu_cpy_hdr_t cpy_hdr;
	mvrlu_obj_hdr_t obj_hdr;
} __packed mvrlu_cpy_hdr_struct_t;

typedef struct mvrlu_thread_struct mvrlu_thread_struct_t;

typedef struct mvrlu_wrt_set {
	volatile unsigned long wrt_clk;
	volatile unsigned int num_objs;
	unsigned long start_tail_cnt;
	mvrlu_thread_struct_t *thread;
} ____ptr_aligned mvrlu_wrt_set_t;

typedef struct mvrlu_wrt_set_struct {
	mvrlu_cpy_hdr_struct_t chs;
	mvrlu_wrt_set_t wrt_set;
} __packed mvrlu_wrt_set_struct_t;

typedef struct mvrlu_log {
	volatile unsigned long qp_clk1;
	volatile unsigned long qp_clk2;
	volatile unsigned int reclaim_lock;
	volatile unsigned int need_reclaim;
	volatile unsigned long head_cnt;
	volatile unsigned long tail_cnt;
	mvrlu_wrt_set_t *cur_wrt_set;

	long __padding_0[MVRLU_DEFAULT_PADDING];
	volatile unsigned char *buffer;
} mvrlu_log_t;

typedef struct mvrlu_free_ptrs {
	unsigned int num_ptrs;
	void *ptrs[MVRLU_MAX_FREE_PTRS]; /* p_act */
} mvrlu_free_ptrs_t;

typedef struct mvrlu_qp_info {
	unsigned int need_wait;
	unsigned int run_cnt;
} mvrlu_qp_info_t;

typedef struct mvrlu_list {
	struct mvrlu_list *next, *prev;
} mvrlu_list_t;

typedef struct mvrlu_thread_struct {
	long __padding_0[MVRLU_DEFAULT_PADDING];

	unsigned int tid;
	int is_write_detected;
	mvrlu_free_ptrs_t free_ptrs;

#ifdef MVRLU_ENABLE_STATS
	mvrlu_stat_t stat;
#endif

	long __padding_1[MVRLU_DEFAULT_PADDING];

	volatile unsigned int run_cnt;
	volatile unsigned long local_clk;
	volatile int live_status;

	long __padding_2[MVRLU_DEFAULT_PADDING];

	mvrlu_qp_info_t qp_info;
	mvrlu_log_t log;

	long __padding_3[MVRLU_DEFAULT_PADDING];

	int num_act_obj;
	int num_deref;

	long __padding_4[MVRLU_DEFAULT_PADDING];

	mvrlu_list_t list;
} mvrlu_thread_struct_t;

typedef struct mvrlu_thread_list {
#ifdef __KERNEL__
	spinlock_t lock;
#else
	pthread_spinlock_t lock;
#endif

	long __padding_0[MVRLU_DEFAULT_PADDING];

	volatile int thread_wait;
	unsigned int cur_tid;
	unsigned int num;
	mvrlu_list_t list;
} mvrlu_thread_list_t;

typedef struct mvrlu_qp_thread {
	unsigned long qp_clk;

#ifdef __KERNEL__
	struct task_struct *thread;
	struct completion completion;
	struct mutex cond_mutex;
	struct completion cond;
#else
	pthread_t thread;
	intptr_t completion;
	pthread_mutex_t cond_mutex;
	pthread_cond_t cond;
#endif

	volatile int stop_requested;
	volatile int need_reclaim;

#ifdef MVRLU_ENABLE_STATS
	mvrlu_stat_t stat;
#endif
} mvrlu_qp_thread_t;

#endif /* _MVRLU_I_H */
