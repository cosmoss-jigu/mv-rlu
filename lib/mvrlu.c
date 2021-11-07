// SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
// SPDX-License-Identifier: Apache-2.0
#ifndef __KERNEL__
#include "mvrlu.h"
#else
#include <linux/mvrlu.h>
#endif

#include "mvrlu_i.h"
#include "debug.h"
#include "port.h"

/*
 * Global data structures
 */
static mvrlu_thread_list_t g_live_threads ____cacheline_aligned2;
static mvrlu_thread_list_t g_zombie_threads ____cacheline_aligned2;
static mvrlu_qp_thread_t g_qp_thread ____cacheline_aligned2;

#ifdef MVRLU_ENABLE_STATS
static mvrlu_stat_t g_stat ____cacheline_aligned2;
#endif

/*
 * Forward declarations
 */

static void qp_update_qp_clk_for_reclaim(mvrlu_qp_thread_t *qp_thread,
					 mvrlu_thread_struct_t *thread);
static int wakeup_qp_thread_for_reclaim(void);
static void print_config(void);

/*
 * Clock-related functions
 */

#ifndef MVRLU_ORDO_TIMESTAMPING
static volatile unsigned long
	__g_wrt_clk[2 * CACHE_DEFAULT_PADDING] ____cacheline_aligned2;
#define g_wrt_clk __g_wrt_clk[CACHE_DEFAULT_PADDING]
#define gte_clock(__t1, __t2) ((__t1) >= (__t2))
#define lte_clock(__t1, __t2) ((__t1) <= (__t2))
#define get_clock() g_wrt_clk
#define get_clock_relaxed() get_clock()
#define init_clock()                                                           \
	do {                                                                   \
		g_wrt_clk = 0;                                                 \
	} while (0)
#define new_clock(__x) (g_wrt_clk + 1)
#define advance_clock() smp_faa(&g_wrt_clk, 1)
#define correct_qp_clk(qp_clk) qp_clk
#else /* MVRLU_ORDO_TIMESTAMPING */
#include "ordo_clock.h"
#define gte_clock(__t1, __t2) ordo_gt_clock(__t1, __t2)
#define lte_clock(__t1, __t2) ordo_lt_clock(__t1, __t2)
#define get_clock() ordo_get_clock()
#define get_clock_relaxed() ordo_get_clock_relaxed()
#define init_clock() ordo_clock_init()
#define new_clock(__local_clk) ordo_new_clock((__local_clk) + ordo_boundary())
#define advance_clock()
#define correct_qp_clk(qp_clk) qp_clk - ordo_boundary()
#endif /* MVRLU_ORDO_TIMESTAMPING */

/*
 * Utility functions
 */

#define log_to_thread(__log)                                                   \
	({                                                                     \
		void *p = (void *)(__log);                                     \
		void *q;                                                       \
		q = p - ((size_t) & ((mvrlu_thread_struct_t *)0)->log);        \
		(mvrlu_thread_struct_t *)q;                                    \
	})

#define list_to_thread(__list)                                                 \
	({                                                                     \
		void *p = (void *)(__list);                                    \
		void *q;                                                       \
		q = p - ((size_t) & ((mvrlu_thread_struct_t *)0)->list);       \
		(mvrlu_thread_struct_t *)q;                                    \
	})

#define chs_to_thread(__chs)                                                   \
	({                                                                     \
		void *p = (void *)(__chs)->cpy_hdr.p_wrt_clk;                  \
		void *q;                                                       \
		q = p - ((size_t) & ((mvrlu_wrt_set_t *)0)->wrt_clk);          \
		((mvrlu_wrt_set_t *)q)->thread;                                \
	})

static inline void assert_chs_type(const mvrlu_cpy_hdr_struct_t *chs)
{
	mvrlu_assert(chs->obj_hdr.type == TYPE_WRT_SET ||
		     chs->obj_hdr.type == TYPE_COPY ||
		     chs->obj_hdr.type == TYPE_FREE ||
		     chs->obj_hdr.type == TYPE_BOGUS);
}

/*
 * Statistics functions
 */

#ifdef MVRLU_ENABLE_STATS
#define stat_thread_inc(self, x) stat_inc(&(self)->stat, stat_##x)
#define stat_thread_acc(self, x, y) stat_acc(&(self)->stat, stat_##x, y)
#define stat_thread_max(self, x, y) stat_max(&(self)->stat, stat_##x, y)
#define stat_qp_inc(qp, x) stat_inc(&(qp)->stat, stat_##x)
#define stat_qp_acc(qp, x, y) stat_acc(&(qp)->stat, stat_##x, y)
#define stat_qp_max(qp, x, y) stat_max(&(qp)->stat, stat_##x, y)
#define stat_log_inc(log, x) stat_thread_inc(log_to_thread(log), x)
#define stat_log_acc(log, x, y) stat_thread_acc(log_to_thread(log), x, y)
#define stat_log_max(log, x, y) stat_thread_max(log_to_thread(log), x, y)
#define stat_thread_merge(self) stat_atomic_merge(&g_stat, &(self)->stat)
#define stat_qp_merge(qp) stat_atomic_merge(&g_stat, &(qp)->stat)
#else /* MVRLU_ENABLE_STATS */
#define stat_thread_inc(self, x)
#define stat_thread_acc(self, x, y)
#define stat_thread_max(self, x, y)
#define stat_qp_inc(qp, x)
#define stat_qp_acc(qp, x, y)
#define stat_qp_max(qp, x, y)
#define stat_log_inc(log, x)
#define stat_log_acc(log, x, y)
#define stat_log_max(log, x, y)
#define stat_thread_merge(self)
#define stat_qp_merge(qp)
#endif /* MVRLU_ENABLE_STATS */

static const char *stat_get_name(int s)
{
/*
	 * Check out following implementation tricks:
	 * - C preprocessor applications
	 *   https://bit.ly/2H1sC5G
	 * - Stringification
	 *   https://gcc.gnu.org/onlinedocs/gcc-4.1.2/cpp/Stringification.html
	 * - Designated Initializers in C
	 *   https://www.geeksforgeeks.org/designated-initializers-c/
	 */
#undef S
#define S(x) [stat_##x] = #x,
	static const char *stat_string[stat_max__ + 1] = { STAT_NAMES };

	mvrlu_assert(s >= 0 && s < stat_max__);
	return stat_string[s];
}

static void stat_print_cnt(mvrlu_stat_t *stat)
{
	int i;
	for (i = 0; i < stat_max__; ++i) {
		printf("  %30s = %lu\n", stat_get_name(i), stat->cnt[i]);
	}
}

static void stat_reset(mvrlu_stat_t *stat)
{
	int i;
	for (i = 0; i < stat_max__; ++i) {
		stat->cnt[i] = 0;
	}
}
static void stat_atomic_merge(mvrlu_stat_t *tgt, mvrlu_stat_t *src)
{
	int i;
	for (i = 0; i < stat_max__; ++i) {
		smp_faa(&tgt->cnt[i], src->cnt[i]);
	}
}

static inline void stat_inc(mvrlu_stat_t *stat, int s)
{
	stat->cnt[s]++;
}

static inline void stat_acc(mvrlu_stat_t *stat, int s, unsigned long v)
{
	stat->cnt[s] += v;
}

static inline void stat_max(mvrlu_stat_t *stat, int s, unsigned long v)
{
	if (v > stat->cnt[s])
		stat->cnt[s] = v;
}

/*
 * thread information
 */

static inline void init_mvrlu_list(mvrlu_list_t *list)
{
	list->next = list;
	list->prev = list;
}

static inline void mvrlu_list_add(mvrlu_list_t *new, mvrlu_list_t *head)
{
	head->next->prev = new;
	new->next = head->next;
	new->prev = head;
	head->next = new;
}

static inline void mvrlu_list_del(mvrlu_list_t *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

static inline int mvrlu_list_empty(const mvrlu_list_t *head)
{
	return head->next == head && head->prev == head;
}

static inline void mvrlu_list_rotate_left(mvrlu_list_t *head)
{
	/* Rotate a list in counterclockwise direction:
	 *
	 * Before rotation:
	 *  [T3]->{{H}}->[T0]->[T1]->[T2]
	 *  /|\                       |
	 *   +------------------------+
	 *
	 * After rotation:
	 *  [T3]->[T0]->{{H}}->[T1]->[T2]
	 *  /|\                       |
	 *   +------------------------+
	 */
	if (!mvrlu_list_empty(head)) {
		mvrlu_list_t *first;
		first = head->next;
		mvrlu_list_del(first);
		mvrlu_list_add(first, head->prev);
	}
}

#define thread_list_for_each_safe(tl, pos, n, thread)                          \
	for (pos = (tl)->list.next, n = (pos)->next,                           \
	    thread = list_to_thread(pos);                                      \
	     pos != &(tl)->list;                                               \
	     pos = n, n = (pos)->next, thread = list_to_thread(pos))

static inline int thread_list_has_waiter(mvrlu_thread_list_t *tl)
{
	return tl->thread_wait;
}

static inline void init_thread_list(mvrlu_thread_list_t *tl)
{
	port_spin_init(&tl->lock);

	tl->cur_tid = 0;
	tl->num = 0;
	init_mvrlu_list(&tl->list);
}

static inline void thread_list_destroy(mvrlu_thread_list_t *tl)
{
	port_spin_destroy(&tl->lock);
}

static inline void thread_list_lock(mvrlu_thread_list_t *tl)
{
	/* Lock acquisition with a normal priority */
	port_spin_lock(&tl->lock);
}

static inline void thread_list_lock_force(mvrlu_thread_list_t *tl)
{
	/* Lock acquisition with a high priority
	 * which turns on the thread_wait flag
	 * so a lengthy task can stop voluntarily
	 * stop and resume later. */
	if (!port_spin_trylock(&tl->lock)) {
		smp_cas(&tl->thread_wait, 0, 1);
		port_spin_lock(&tl->lock);
	}
}

static inline void thread_list_unlock(mvrlu_thread_list_t *tl)
{
	if (tl->thread_wait)
		smp_atomic_store(&tl->thread_wait, 0);
	port_spin_unlock(&tl->lock);
}

static inline void thread_list_add(mvrlu_thread_list_t *tl,
				   mvrlu_thread_struct_t *self)
{
	thread_list_lock_force(tl);
	{
		self->tid = tl->cur_tid++;
		tl->num++;
		mvrlu_list_add(&self->list, &tl->list);
	}
	thread_list_unlock(tl);
}

static inline void thread_list_del_unsafe(mvrlu_thread_list_t *tl,
					  mvrlu_thread_struct_t *self)
{
	tl->num--;
	mvrlu_list_del(&self->list);
	self->list.prev = self->list.next = NULL;
}

static inline void thread_list_del(mvrlu_thread_list_t *tl,
				   mvrlu_thread_struct_t *self)
{
	thread_list_lock_force(tl);
	{
		thread_list_del_unsafe(tl, self);
	}
	thread_list_unlock(tl);
}

static inline void thread_list_rotate_left_unsafe(mvrlu_thread_list_t *tl)
{
	/* NOTE: A caller should hold a lock */
	mvrlu_list_rotate_left(&tl->list);
}

/*
 * Object access functions
 */

static inline mvrlu_act_hdr_struct_t *obj_to_ahs(void *obj)
{
	mvrlu_act_hdr_struct_t *ahs = obj;
	return &ahs[-1];
}

static inline mvrlu_act_hdr_struct_t *vobj_to_ahs(volatile void *vobj)
{
	return obj_to_ahs((void *)vobj);
}

static inline mvrlu_cpy_hdr_struct_t *obj_to_chs(void *obj)
{
	mvrlu_cpy_hdr_struct_t *chs = obj;
	return &chs[-1];
}

static inline mvrlu_cpy_hdr_struct_t *vobj_to_chs(volatile void *vobj)
{
	return obj_to_chs((void *)vobj);
}

static inline mvrlu_obj_hdr_t *obj_to_obj_hdr(void *obj)
{
	mvrlu_obj_hdr_t *ohdr = obj;
	return &ohdr[-1];
}

static inline mvrlu_obj_hdr_t *vobj_to_obj_hdr(volatile void *vobj)
{
	return obj_to_obj_hdr((void *)vobj);
}

static inline int is_obj_actual(mvrlu_obj_hdr_t *obj_hdr)
{
#ifdef MVRLU_DISABLE_ADDR_ACTUAL_TYPE_CHECKING
	/* Test object type based on its type information
	 * in the header. It may cause one cache miss. */
	return obj_hdr->type == TYPE_ACTUAL;
#else
	/* Test if an object is in the log region or not.
	 * If not, it is an actual object. We avoid one
	 * memory reference so we may avoid one cache miss. */
	return !port_addr_in_log_region(obj_hdr);
#endif /* MVRLU_DISABLE_ADDR_ACTUAL_TYPE_CHECKING */
}

static inline void *get_act_obj(void *obj)
{
	mvrlu_obj_hdr_t *obj_hdr = obj_to_obj_hdr(obj);

	if (likely(is_obj_actual(obj_hdr)))
		return obj;
	return (void *)obj_to_chs(obj)->cpy_hdr.p_act;
}

static inline unsigned int align_uint_to_cacheline(unsigned int unum)
{
	return (unum + ~MVRLU_CACHE_LINE_MASK) & MVRLU_CACHE_LINE_MASK;
}

static inline void *align_ptr_to_cacheline(void *p)
{
	return (void *)(((unsigned long)p + ~MVRLU_CACHE_LINE_MASK) &
			MVRLU_CACHE_LINE_MASK);
}

/*
 * Object copy operations
 */

static inline unsigned int get_log_size(const mvrlu_cpy_hdr_struct_t *chs)
{
	return chs->obj_hdr.obj_size + chs->obj_hdr.padding_size;
}

static inline unsigned long get_wrt_clk(const mvrlu_cpy_hdr_struct_t *chs)
{
	unsigned long wrt_clk;

	wrt_clk = chs->cpy_hdr.__wrt_clk;
	if (unlikely(wrt_clk == MAX_VERSION)) {
		smp_rmb();
		wrt_clk = *chs->cpy_hdr.p_wrt_clk;
	}
	return wrt_clk;
}

static int try_lock_obj(mvrlu_act_hdr_struct_t *ahs, volatile void *p_old_copy,
			volatile void *p_new_copy)
{
	int ret;

	if (ahs->act_hdr.p_lock != NULL || ahs->obj_hdr.p_copy != p_old_copy)
		return 0;

	ret = smp_cas(&ahs->act_hdr.p_lock, NULL, p_new_copy);
	if (!ret)
		return 0; /* smp_cas() failed */

	if (unlikely(ahs->obj_hdr.p_copy != p_old_copy)) {
		/* If it is ABA, unlock and return false */
		smp_wmb();
		ahs->act_hdr.p_lock = NULL;
		return 0;
	}

	/* Finally succeeded. Updating p_copy of p_new_copy
	 * will be done upon commit. */
	return 1;
}

static void try_detach_obj(mvrlu_cpy_hdr_struct_t *chs)
{
	mvrlu_act_hdr_struct_t *ahs;
	void *p_act, *p_copy;

	/* If the object is the latest object after qp2, the
	 * p_copy needs to be set to NULL */

	/* Copy to the actual object when it is the latest copy */
	p_act = (void *)chs->cpy_hdr.p_act;
	ahs = obj_to_ahs(p_act);
	p_copy = (void *)chs->obj_hdr.obj;
	if (ahs->obj_hdr.p_copy != p_copy)
		return;

	/* Set p_copy of the actual object to NULL */
	if (smp_cas(&ahs->obj_hdr.p_copy, p_copy, NULL)) {
		/* Succeed in detaching the object */
		return;
	}
}

static int try_writeback_obj(mvrlu_cpy_hdr_struct_t *chs)
{
	mvrlu_act_hdr_struct_t *ahs;
	void *p_act, *p_copy;

	/* Copy to the actual object when it is the latest copy */
	p_act = (void *)chs->cpy_hdr.p_act;
	ahs = obj_to_ahs(p_act);
	p_copy = (void *)chs->obj_hdr.obj;
	if (ahs->obj_hdr.p_copy != p_copy)
		return 0;

	/* Write back the copy to the master */
	memcpy(p_act, p_copy, chs->obj_hdr.obj_size);
	smp_wmb_tso();
	return 1;
}

static void free_obj(mvrlu_cpy_hdr_struct_t *chs)
{
#ifdef MVRLU_ENABLE_FREE_POISIONING
	mvrlu_act_hdr_struct_t *ahs;
	ahs = vobj_to_ahs(chs->cpy_hdr.p_act);
	memset((void *)chs->cpy_hdr.p_act, MVRLU_FREE_POSION,
	       ahs->obj_hdr.obj_size);
	memset((void *)ahs, MVRLU_FREE_POSION, sizeof(*ahs));
#endif

	port_free(vobj_to_ahs(chs->cpy_hdr.p_act));
}

/*
 * Log operations
 */

static inline unsigned long log_used(mvrlu_log_t *log)
{
	unsigned long used = log->tail_cnt - log->head_cnt;
	stat_log_max(log, max_log_used_bytes, used);
	return used;
}

static inline unsigned int log_index(unsigned long cnt)
{
	return cnt & ~MVRLU_LOG_MASK;
}

static inline void *log_at(mvrlu_log_t *log, unsigned long cnt)
{
	return (void *)&log->buffer[log_index(cnt)];
}

static inline mvrlu_wrt_set_struct_t *log_at_wss(mvrlu_log_t *log,
						 unsigned long cnt)
{
	mvrlu_wrt_set_struct_t *wss;
	wss = (mvrlu_wrt_set_struct_t *)log_at(log, cnt);
	mvrlu_assert(wss->chs.obj_hdr.type == TYPE_WRT_SET);
	return wss;
}

static inline mvrlu_cpy_hdr_struct_t *log_at_chs(mvrlu_log_t *log,
						 unsigned long cnt)
{
	mvrlu_cpy_hdr_struct_t *chs;
	chs = (mvrlu_cpy_hdr_struct_t *)log_at(log, cnt);
	mvrlu_assert(chs == align_ptr_to_cacheline(chs));
	return chs;
}

static inline unsigned int add_extra_padding(mvrlu_log_t *log,
					     unsigned int log_size,
					     unsigned int extra_size, int bogus)
{
	unsigned int padding = 0;

	extra_size = extra_size + sizeof(mvrlu_cpy_hdr_struct_t);
	extra_size = align_uint_to_cacheline(extra_size);

	if (bogus == 0 && log_index(log->tail_cnt + log_size + extra_size) <
				  log_index(log->tail_cnt)) {
		padding = MVRLU_LOG_SIZE - log_index(log->tail_cnt + log_size);
	}
	if (padding) {
		mvrlu_assert(log_index(log->tail_cnt + log_size + padding) ==
			     0);
	}

	return padding;
}
static mvrlu_cpy_hdr_struct_t *log_alloc(mvrlu_log_t *log,
					 unsigned int obj_size, int *bogus)
{
	mvrlu_cpy_hdr_struct_t *chs;
	mvrlu_wrt_set_struct_t *wss;
	unsigned int log_size;
	unsigned int extra_pad;

	/* Make log_size cacheline aligned */
	log_size = obj_size + sizeof(mvrlu_cpy_hdr_struct_t);
	log_size = align_uint_to_cacheline(log_size);
	mvrlu_assert(log_size < MVRLU_LOG_SIZE);

	/* If an allocation wraps around the end of a log,
	 * insert a bogus object to prevent such case in real
	 * object access.
	 *
	 *   +-----------------------------------+
	 *   |                             |bogus|
	 *   +-----------------------------------+
	 *      \                           \
	 *       \                           +- 1) log->tail_cnt
	 *        +- 2) log->tail_cnt + log_size
	 */

	if (log_index(log->tail_cnt + log_size) < log_index(log->tail_cnt)) {
		unsigned int bogus_size;

		chs = log_at(log, log->tail_cnt);
		memset(chs, 0, sizeof(*chs));
		bogus_size = MVRLU_LOG_SIZE - log_index(log->tail_cnt);
		chs->obj_hdr.padding_size = bogus_size;
		chs->obj_hdr.type = TYPE_BOGUS;

		log->tail_cnt += bogus_size;
		mvrlu_assert(log_index(log->tail_cnt) == 0);
		*bogus = 1;
	}
	mvrlu_assert(log_index(log->tail_cnt) <
		     log_index(log->tail_cnt + log_size));
	extra_pad = add_extra_padding(log, log_size, sizeof(*wss), *bogus);
	log_size += extra_pad;

	/*
	 *   +--- mvrlu_cpy_hdr_struct_t ---+
	 *  /                                \
	 * +---------------------------------------------+----
	 * | mvrlu_cpy_hdr_t | mvrlu_obj_hdr  | copy obj | ...
	 * +---------------------------------------------+----
	 */
	chs = log_at(log, log->tail_cnt);
	memset(chs, 0, sizeof(*chs));
	chs->cpy_hdr.__wrt_clk = MAX_VERSION;
	chs->obj_hdr.obj_size = obj_size;
	chs->obj_hdr.padding_size = log_size - obj_size;
	return chs;
}

static mvrlu_cpy_hdr_struct_t *log_append_begin(mvrlu_log_t *log,
						volatile void *p_act,
						unsigned int obj_size,
						int *bogus)
{
	mvrlu_cpy_hdr_struct_t *chs;
	*bogus = 0;

	/* Add a write set if it is not allocated */
	if (unlikely(!log->cur_wrt_set)) {
		mvrlu_wrt_set_t *ws;
		mvrlu_assert(log_index(log->tail_cnt) <
			     log_index(log->tail_cnt + sizeof(*ws)));
		chs = log_alloc(log, sizeof(*ws), bogus);
		ws = (mvrlu_wrt_set_t *)chs->obj_hdr.obj;
		chs->cpy_hdr.p_wrt_clk = &ws->wrt_clk;
		chs->obj_hdr.type = TYPE_WRT_SET;
		ws->wrt_clk = MAX_VERSION;
		ws->num_objs = 0;
		ws->start_tail_cnt = log->tail_cnt;
		ws->thread = log_to_thread(log);
		log->cur_wrt_set = ws;
		log->tail_cnt += get_log_size(chs);
		mvrlu_assert(*bogus == 0);
	}

	/* allocate an object */
	chs = log_alloc(log, obj_size, bogus);
	chs->cpy_hdr.p_wrt_clk = &log->cur_wrt_set->wrt_clk;
	chs->cpy_hdr.p_act = p_act;
	chs->obj_hdr.type = TYPE_COPY;
	return chs;
}

static inline void log_append_abort(mvrlu_log_t *log,
				    mvrlu_cpy_hdr_struct_t *chs)
{
	/* Do nothing since tail_cnt and num_objs are not updated yet.
	 * Let's keep this for readability of the code. */
}

static inline void log_append_end(mvrlu_log_t *log, mvrlu_cpy_hdr_struct_t *chs,
				  int bogus_allocated)
{
	log->tail_cnt += get_log_size(chs);
	log->cur_wrt_set->num_objs++;
	if (bogus_allocated) {
		log->cur_wrt_set->num_objs++;
	}
}

static inline unsigned long ws_iter_begin(mvrlu_wrt_set_t *ws)
{
	mvrlu_cpy_hdr_struct_t *chs;

	chs = obj_to_chs(ws);
	return ws->start_tail_cnt + get_log_size(chs);
}

static inline unsigned long ws_iter_next(unsigned long iter,
					 mvrlu_cpy_hdr_struct_t *chs)
{
	return iter + get_log_size(chs);
}

static inline int fp_is_free(mvrlu_free_ptrs_t *free_ptrs,
			     mvrlu_act_hdr_struct_t *ahs)
{
	void *p_act;
	unsigned int i;

	p_act = ahs->obj_hdr.obj;
	for (i = 0; i < free_ptrs->num_ptrs; ++i) {
		if (free_ptrs->ptrs[i] == p_act)
			return 1;
	}
	return 0;
}

static inline void fp_reset(mvrlu_free_ptrs_t *free_ptrs)
{
	free_ptrs->num_ptrs = 0;
}

#define ws_for_each(log, ws, obj_idx, log_cnt)                                 \
	for ((obj_idx) = 0, (log_cnt) = ws_iter_begin(ws);                     \
	     (obj_idx) < (ws)->num_objs;                                       \
	     ++(obj_idx), (log_cnt) = ws_iter_next(log_cnt, chs))

static void ws_move_lock_to_copy(mvrlu_log_t *log, mvrlu_free_ptrs_t *free_ptrs)
{
	mvrlu_wrt_set_t *ws;
	mvrlu_cpy_hdr_struct_t *chs;
	unsigned long cnt;
	unsigned long wrt_clk_next;
	unsigned int num_free;
	unsigned int i;

	ws = log->cur_wrt_set;
	num_free = free_ptrs->num_ptrs;
	ws_for_each (log, ws, i, cnt) {
		mvrlu_act_hdr_struct_t *ahs;
		volatile void *p_old_copy, *p_old_copy2;

		chs = log_at_chs(log, cnt);
		assert_chs_type(chs);
		if (unlikely(chs->obj_hdr.type == TYPE_BOGUS)) {
			continue;
		}
		ahs = vobj_to_ahs(chs->cpy_hdr.p_act);
		mvrlu_assert(chs->obj_hdr.type == TYPE_COPY);
		mvrlu_assert(ahs->act_hdr.p_lock == chs->obj_hdr.obj);

		/* If an object is free()-ed, change its type. */
		if (unlikely(num_free > 0 && fp_is_free(free_ptrs, ahs))) {
			chs->obj_hdr.type = TYPE_FREE;
			--num_free;
			/* Freed copy should not be accessible
			 * from the version chain. */
			continue;
		}

		/* If the size of copied object is zero, that is
		 * for try_lock_const() so we do not insert it
		 * to the version list. */
		if (!chs->obj_hdr.obj_size)
			continue;

		/* Move a locked object to the version chain
		 * of an actual object. */
		p_old_copy = ahs->obj_hdr.p_copy;

		while (1) {
			/* Initialize p_copy and wrt_clk_next. */
			chs->obj_hdr.p_copy = p_old_copy;
			if (p_old_copy == NULL)
				chs->cpy_hdr.wrt_clk_next = MIN_VERSION;
			else {
				wrt_clk_next =
					get_wrt_clk(vobj_to_chs(p_old_copy));
				chs->cpy_hdr.wrt_clk_next = wrt_clk_next;
			}
			mvrlu_assert(chs->cpy_hdr.wrt_clk_next != MAX_VERSION);
			smp_wmb_tso();

			/* Since p_copy of p_act can be set to NULL upon
			 * reclaim, we should update it using smp_cas(). */
			if (smp_cas_v(&ahs->obj_hdr.p_copy, p_old_copy,
				      chs->obj_hdr.obj, p_old_copy2))
				break;

			/* smp_cas_v() failed. Retry.
			 * p_old_copy2 is updated by smp_cas_v(). */
			p_old_copy = p_old_copy2;
		}
		mvrlu_assert(ahs->obj_hdr.p_copy == chs->obj_hdr.obj);
	}
}

static void ws_unlock(mvrlu_log_t *log, unsigned long wrt_clk)
{
	mvrlu_wrt_set_t *ws;
	mvrlu_cpy_hdr_struct_t *chs;
	unsigned long cnt;
	unsigned int i;

	ws = log->cur_wrt_set;
	wrt_clk = ws->wrt_clk;
	ws_for_each (log, ws, i, cnt) {
		chs = log_at_chs(log, cnt);
		assert_chs_type(chs);
		if (unlikely(chs->obj_hdr.type == TYPE_BOGUS)) {
			continue;
		}
		mvrlu_assert(chs->obj_hdr.type == TYPE_COPY ||
			     chs->obj_hdr.type == TYPE_FREE);

		/* Mark version */
		if (wrt_clk != MAX_VERSION) {
			chs->cpy_hdr.__wrt_clk = wrt_clk;
			smp_wmb_tso();
		}

		/* Unlock */
		if (likely(chs->obj_hdr.type == TYPE_COPY)) {
			mvrlu_act_hdr_struct_t *ahs;
			ahs = vobj_to_ahs(chs->cpy_hdr.p_act);
			mvrlu_assert(ahs->act_hdr.p_lock == chs->obj_hdr.obj);
			ahs->act_hdr.p_lock = NULL;
		}
	}
}

static void log_commit(mvrlu_log_t *log, mvrlu_free_ptrs_t *free_ptrs,
		       unsigned long local_clk)
{
	mvrlu_assert(log->cur_wrt_set);
	mvrlu_assert(obj_to_chs(log->cur_wrt_set)->obj_hdr.type ==
		     TYPE_WRT_SET);

	/* Move a committed object to its version chain */
	ws_move_lock_to_copy(log, free_ptrs);
	smp_wmb();

	/* Make them public atomically */
	smp_atomic_store(&log->cur_wrt_set->wrt_clk, new_clock(local_clk));

	/* Advance global clock */
	advance_clock();

	/* Unlock objects with marking wrt_clk */
	ws_unlock(log, log->cur_wrt_set->wrt_clk);

	/* Clean up */
	log->cur_wrt_set = NULL;
	fp_reset(free_ptrs);
}

static void log_abort(mvrlu_log_t *log, mvrlu_free_ptrs_t *free_ptrs)
{
	/* Unlock objects without marking wrt_clk */
	ws_unlock(log, MAX_VERSION);

	/* Reset the current write set */
	log->tail_cnt = log->cur_wrt_set->start_tail_cnt;
	log->cur_wrt_set = NULL;
	fp_reset(free_ptrs);
}

static inline int try_lock(volatile unsigned int *lock)
{
	if (*lock == 0 && smp_cas(lock, 0, 1))
		return 1;
	return 0;
}

static inline void unlock(volatile unsigned int *lock)
{
	*lock = 0;
}

static void log_reclaim(mvrlu_log_t *log)
{
	/*
	 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	 *                       MV-RLU
	 *                       ======
	 *
	 *                           head'
	 *            head           qp2        qp1    tail
	 *              \           /          /      /
	 *     +---------+====================+------+------+
	 *     |         |..........|/////////|      |      |
	 *     +---------+====================+------+------+
	 *               ~~~~~~~~~~>
	 *               reclaim
	 *                          ~~~~~~~~~~>
	 *                          writeback
	 *
	 * 1. (qp1, tail)
	 *   - The master is still accessible.
	 * 2. (qp2, qp1]
	 *   - Nobody accesses the master because all threads
	 *   access the copy. Thus, we can safely write back
	 *   the copy to the master.
	 *   - Write back only if the copy is the latest.
	 *   - While nobody accesses the master, the copy is
	 *   still accessible. So we cannot free the master yet.
	 * 3. [head, qp2]
	 *   - Nobobdy accesses this copy because a thread
	 *   accesses either of the master or a newer copy.
	 *   Thus, we can safely reclaim this copy.
	 *   - Since nobody accesses either the master
	 *   or the copy, we can free the master.
	 * 4. head' = qp2
	 *   - head is updated to qp2 because log reclamation
	 *   will resume from qp2.
	 *
	 */

	mvrlu_wrt_set_t *ws;
	mvrlu_wrt_set_struct_t *wss;
	mvrlu_cpy_hdr_struct_t *chs;
	unsigned long cnt;
	unsigned long qp_clk1;
	unsigned long qp_clk2;
	unsigned int i;
	unsigned long start_cnt;
	unsigned long tail_cnt;
	unsigned int index;
	int reclaim;
	int try_writeback;

	if (!log->need_reclaim)
		return;
	if (!try_lock(&log->reclaim_lock))
		return;

	qp_clk1 = log->qp_clk1;
	qp_clk2 = log->qp_clk2;
	start_cnt = log->head_cnt;
	tail_cnt = log->tail_cnt;
	while (start_cnt < tail_cnt) {
		reclaim = 0;
		try_writeback = 0;
		index = log_index(start_cnt);
		wss = log_at_wss(log, index);
		ws = &(wss->wrt_set);

		if (gte_clock(ws->wrt_clk, qp_clk1) && ws->wrt_clk != qp_clk1)
			break;
		else if (lte_clock(ws->wrt_clk, qp_clk2))
			reclaim = 1;
		else if (lte_clock(ws->wrt_clk, qp_clk1))
			try_writeback = 1;

		ws_for_each (log, ws, i, cnt) {
			chs = log_at_chs(log, cnt);
			assert_chs_type(chs);
			switch (chs->obj_hdr.type) {
			case TYPE_COPY:
				if (try_writeback && try_writeback_obj(chs))
					try_detach_obj(chs);
				stat_log_inc(log, n_reclaim_copy);
				break;
			case TYPE_FREE:
				if (reclaim) {
					free_obj(chs);
					stat_log_inc(log, n_reclaim_free);
				}
				break;
			case TYPE_BOGUS:
				break; /* Do nothing */
			default:
				mvrlu_assert(0 && "Never be here");
				break;
			}
			mvrlu_assert(cnt <= log->tail_cnt);
		}
		start_cnt = cnt;
		mvrlu_assert(start_cnt <= log->tail_cnt);
		if (reclaim)
			log->head_cnt = start_cnt;
		stat_log_inc(log, n_reclaim_wrt_set);
	}
	stat_log_inc(log, n_reclaim);
	log->need_reclaim = 0;

	unlock(&log->reclaim_lock);
}

static void log_reclaim_force(mvrlu_log_t *log)
{
	if (log->need_reclaim) {
		log_reclaim(log);
		return;
	}

	if (log->head_cnt != log->tail_cnt) {
		int count = 0; /* TODO FIXME */
		wakeup_qp_thread_for_reclaim();
		do {
			port_cpu_relax_and_yield();
			smp_mb();
			count++;
			if (count == 1000) {
				wakeup_qp_thread_for_reclaim();
				count = 0;
			}
		} while (!log->need_reclaim);
		log_reclaim(log);
	}
}

/*
 * Quiescent detection functions
 */

static void qp_init(mvrlu_qp_thread_t *qp_thread, unsigned long qp_clk)
{
	mvrlu_thread_struct_t *thread;
	mvrlu_list_t *pos, *n;

	thread_list_lock(&g_live_threads);
	{
		thread_list_for_each_safe (&g_live_threads, pos, n, thread) {
			thread->qp_info.run_cnt = thread->run_cnt;
			thread->qp_info.need_wait =
				thread->qp_info.run_cnt & 0x1;
		}
	}
	thread_list_unlock(&g_live_threads);
}

static void qp_wait(mvrlu_qp_thread_t *qp_thread, unsigned long qp_clk)
{
	mvrlu_thread_struct_t *thread;
	mvrlu_list_t *pos, *n;

retry:
	thread_list_lock(&g_live_threads);
	{
		thread_list_for_each_safe (&g_live_threads, pos, n, thread) {
			if (!thread->qp_info.need_wait)
				continue;

			while (1) {
				/* Check if a thread passed quiescent period. */
				if (thread->qp_info.run_cnt !=
					    thread->run_cnt ||
				    gte_clock(thread->local_clk, qp_clk)) {
					thread->qp_info.need_wait = 0;
					break;
				}

				/* If a thread is waiting for adding or deleting
				 * from/to the thread list, yield and retry. */
				if (thread_list_has_waiter(&g_live_threads)) {
					thread_list_unlock(&g_live_threads);
					goto retry;
				}

				port_cpu_relax_and_yield();
				smp_mb();
			}
		}
	}
	thread_list_unlock(&g_live_threads);
}

static void qp_take_nap(mvrlu_qp_thread_t *qp_thread)
{
	port_initiate_nap(&qp_thread->cond_mutex, &qp_thread->cond,
			  MVRLU_QP_INTERVAL_USEC);
}

static void qp_detect(mvrlu_qp_thread_t *qp_thread)
{
	unsigned long qp_clk;

	qp_clk = get_clock();
	qp_init(qp_thread, qp_clk);
	stat_qp_inc(qp_thread, n_qp_detect);
	if (!qp_thread->need_reclaim) {
		qp_take_nap(qp_thread);
		stat_qp_inc(qp_thread, n_qp_nap);
	}
	qp_wait(qp_thread, qp_clk);
	qp_thread->qp_clk = correct_qp_clk(qp_clk);
}

static void qp_help_reclaim_log(mvrlu_qp_thread_t *qp_thread)
{
	mvrlu_thread_struct_t *thread;
	mvrlu_list_t *pos, *n;

retry:
	thread_list_lock(&g_live_threads);
	{
		smp_mb();
		thread_list_for_each_safe (&g_live_threads, pos, n, thread) {
			/* If a thread is waiting for adding or deleting
			 * from/to the thread list, yield and retry. */
			if (thread_list_has_waiter(&g_live_threads)) {
				thread_list_unlock(&g_live_threads);
				goto retry;
			}

			/* Help reclaiming */
			if (thread->log.need_reclaim) {
				log_reclaim(&thread->log);
				stat_qp_inc(qp_thread, n_qp_help_reclaim);
			}
		}

		/* Rotate the thread list counter clockwise for fairness. */
		thread_list_rotate_left_unsafe(&g_live_threads);
	}
	thread_list_unlock(&g_live_threads);
}

static void qp_reap_zombie_threads(mvrlu_qp_thread_t *qp_thread)
{
	mvrlu_thread_struct_t *thread;
	mvrlu_list_t *pos, *n;

retry:
	thread_list_lock(&g_zombie_threads);
	{
		smp_mb();
		thread_list_for_each_safe (&g_zombie_threads, pos, n, thread) {
			/* If a thread is waiting for adding or deleting
			 * from/to the thread list, yield and retry. */
			if (thread_list_has_waiter(&g_zombie_threads)) {
				thread_list_unlock(&g_zombie_threads);
				goto retry;
			}

			/* Enforce reclaiming logs */
			qp_update_qp_clk_for_reclaim(qp_thread, thread);
			log_reclaim(&thread->log);

			/* If the log is completely reclaimed, try next thread */
			if (thread->log.head_cnt != thread->log.tail_cnt)
				continue;

			/* Free log buffer if it is not yet freed */
			if (thread->log.buffer) {
				port_free_log_mem((void *)thread->log.buffer);
				thread->log.buffer = NULL;
				stat_thread_merge(thread);
				stat_qp_inc(qp_thread, n_qp_zombie_reclaim);
			}

			/* If it is a dead zombie, reap */
			if (thread->live_status == THREAD_DEAD_ZOMBIE) {
				thread_list_del_unsafe(&g_zombie_threads,
						       thread);
				mvrlu_thread_free(thread);
			}
		}
	}
	thread_list_unlock(&g_zombie_threads);
}

static int qp_check_reclaim_done(mvrlu_qp_thread_t *qp_thread)
{
	mvrlu_thread_struct_t *thread;
	mvrlu_list_t *pos, *n;
	int rc = 1;

	thread_list_lock(&g_live_threads);
	{
		smp_mb();
		thread_list_for_each_safe (&g_live_threads, pos, n, thread) {
			if (thread->log.need_reclaim) {
				rc = 0;
				break;
			}
		}
	}
	thread_list_unlock(&g_live_threads);
	return rc;
}

static void qp_update_qp_clk_for_reclaim(mvrlu_qp_thread_t *qp_thread,
					 mvrlu_thread_struct_t *thread)
{
	thread->log.qp_clk2 = thread->log.qp_clk1;
	thread->log.qp_clk1 = qp_thread->qp_clk;
	thread->log.need_reclaim = 1;
}

static void qp_trigger_reclaim(mvrlu_qp_thread_t *qp_thread)
{
	mvrlu_thread_struct_t *thread;
	mvrlu_list_t *pos, *n;

	thread_list_lock(&g_live_threads);
	{
		thread_list_for_each_safe (&g_live_threads, pos, n, thread) {
			qp_update_qp_clk_for_reclaim(qp_thread, thread);
		}
	}
	thread_list_unlock(&g_live_threads);
	smp_mb();
}

static void __qp_thread_main(void *arg)
{
	mvrlu_qp_thread_t *qp_thread = arg;
	int reclaim_done;
	int i;

	/* qp detection loop */
	reclaim_done = 1;
	while (!qp_thread->stop_requested) {
		qp_detect(qp_thread);

		if (!reclaim_done) {
			qp_reap_zombie_threads(qp_thread);
			qp_help_reclaim_log(qp_thread);
			reclaim_done = qp_check_reclaim_done(qp_thread);
			if (reclaim_done) {
				smp_cas(&qp_thread->need_reclaim, 1, 0);
				smp_mb();
			}
		}

		if (reclaim_done && qp_thread->need_reclaim) {
			reclaim_done = 0;
			qp_trigger_reclaim(qp_thread);
		}
	}

	/* This is the final reclamation so we should completely reclaim
	 * all logs. To do that, we have to reclaim twice because we need
	 * two qp duration for complete reclamation. */
	for (i = 0; i < 2; ++i) {
		qp_thread->qp_clk = get_clock();
		qp_reap_zombie_threads(qp_thread);
	}
}

#ifdef __KERNEL__
static int qp_thread_main(void *arg)
{
	__qp_thread_main(arg);
	return 0;
}
#else
static void *qp_thread_main(void *arg)
{
	__qp_thread_main(arg);
	return NULL;
}
#endif

static int init_qp_thread(mvrlu_qp_thread_t *qp_thread)
{
	int rc;

	memset(qp_thread, 0, sizeof(*qp_thread));
	port_cond_init(&qp_thread->cond);
	port_mutex_init(&qp_thread->cond_mutex);
	rc = port_create_thread("qp_thread", &qp_thread->thread,
				&qp_thread_main, qp_thread,
				&qp_thread->completion);
	if (rc) {
		mvrlu_trace_global("Error creating builder thread: %d\n", rc);
		return rc;
	}
	return 0;
}

static inline void wakeup_qp_thread(mvrlu_qp_thread_t *qp_thread)
{
	port_initiate_wakeup(&qp_thread->cond_mutex, &qp_thread->cond);
}

static void finish_qp_thread(mvrlu_qp_thread_t *qp_thread)
{
	wakeup_qp_thread(qp_thread);

	smp_atomic_store(&qp_thread->stop_requested, 1);
	smp_mb();

	port_wait_for_finish(&qp_thread->thread, &qp_thread->completion);
	port_mutex_destroy(&qp_thread->cond_mutex);
	port_cond_destroy(&qp_thread->cond);
	stat_qp_merge(qp_thread);
}

static inline int wakeup_qp_thread_for_reclaim(void)
{
	mvrlu_qp_thread_t *qp_thread = &g_qp_thread;

	if (!qp_thread->need_reclaim &&
	    smp_cas(&qp_thread->need_reclaim, 0, 1)) {
		wakeup_qp_thread(qp_thread);
		return 1;
	}
	return 0;
}

/*
 * External APIs
 */

int __init mvrlu_init(void)
{
	static int init = 0;
	int rc;

	/* Compile time sanity check */
	static_assert(sizeof(mvrlu_act_hdr_struct_t) < L1_CACHE_BYTES);
	static_assert(sizeof(mvrlu_cpy_hdr_struct_t) < L1_CACHE_BYTES);
	static_assert((MVRLU_LOG_SIZE & (MVRLU_LOG_SIZE - 1)) == 0);

	/* Make sure whether it is initialized once */
	if (!smp_cas(&init, 0, 1))
		return -EBUSY;

	/* Initialize */
	init_clock();
	init_thread_list(&g_live_threads);
	init_thread_list(&g_zombie_threads);
	rc = port_log_region_init(MVRLU_LOG_SIZE, MVRLU_MAX_THREAD_NUM);
	if (rc) {
		mvrlu_trace_global("Fail to initialize a log region\n");
		return rc;
	}
	rc = init_qp_thread(&g_qp_thread);
	if (rc) {
		mvrlu_trace_global("Fail to initialize a qp thread\n");
		return rc;
	}

	return 0;
}
early_initcall(mvrlu_init);

void mvrlu_finish(void)
{
	finish_qp_thread(&g_qp_thread);
	thread_list_destroy(&g_live_threads);
	thread_list_destroy(&g_zombie_threads);
	port_log_region_destroy();
}

mvrlu_thread_struct_t *mvrlu_thread_alloc(void)
{
	return port_alloc(sizeof(mvrlu_thread_struct_t));
}
EXPORT_SYMBOL(mvrlu_thread_alloc);

void mvrlu_thread_free(mvrlu_thread_struct_t *self)
{
	smp_atomic_store(&self->live_status, THREAD_DEAD_ZOMBIE);

	/* If the log is not completely reclaimed yet,
	 * defer the free until it is completely reclaimed.
	 * In this case, we just move the thread to the zombie
	 * list and the qp thread will eventually reclaim
	 * the log. */
	smp_mb();
	if (self->log.head_cnt == self->log.tail_cnt)
		port_free(self);
}
EXPORT_SYMBOL(mvrlu_thread_free);

void mvrlu_thread_init(mvrlu_thread_struct_t *self)
{
	/* Zero out self */
	memset(self, 0, sizeof(*self));

	/* Allocate cacheline-aligned log space */
	self->log.buffer = port_alloc_log_mem();
	mvrlu_assert(self->log.buffer ==
		     align_ptr_to_cacheline((void *)self->log.buffer));

	/* Add this to the global list */
	thread_list_add(&g_live_threads, self);
	smp_mb();
}
EXPORT_SYMBOL(mvrlu_thread_init);

void mvrlu_thread_finish(mvrlu_thread_struct_t *self)
{
	/* Reclaim data as much as it can */
	if (self->log.need_reclaim)
		log_reclaim(&self->log);

	/* Deregister this thread from the live list */
	self->qp_info.need_wait = 0;
	thread_list_del(&g_live_threads, self);

	/* If the log is empty, free log space and update statistics */
	smp_mb();
	if (self->log.head_cnt == self->log.tail_cnt) {
		port_free_log_mem((void *)self->log.buffer);
		self->log.buffer = NULL;
		stat_thread_merge(self);
	}
	/* Otherwise add it to the zombie list to reclaim the log later */
	else {
		smp_atomic_store(&self->live_status, THREAD_LIVE_ZOMBIE);
		thread_list_add(&g_zombie_threads, self);
	}
}
EXPORT_SYMBOL(mvrlu_thread_finish);

void *mvrlu_alloc_x(size_t size, unsigned int flags)
{
	mvrlu_act_hdr_struct_t *ahs;

	ahs = (void *)port_alloc_x(sizeof(*ahs) + size, flags);
	if (unlikely(ahs == NULL))
		return NULL;

	memset(ahs, 0, sizeof(*ahs));
	ahs->obj_hdr.type = TYPE_ACTUAL;
	ahs->obj_hdr.obj_size = size;
	return ahs->obj_hdr.obj;
}
EXPORT_SYMBOL(mvrlu_alloc_x);

void *mvrlu_alloc(size_t size)
{
	return mvrlu_alloc_x(size, PORT_DEFAULT_ALLOC_FLAG);
}
EXPORT_SYMBOL(mvrlu_alloc);

void mvrlu_free(mvrlu_thread_struct_t *self, void *obj)
{
	void *p_act;

	if (unlikely(obj == NULL))
		return;

	if (unlikely(self == NULL)) {
		port_free(obj_to_ahs(obj));
		return;
	}
	mvrlu_assert(self->run_cnt & 0x1);

	p_act = get_act_obj(obj);
	mvrlu_warning(obj_to_ahs(p_act)->act_hdr.p_lock != NULL);

	self->free_ptrs.ptrs[self->free_ptrs.num_ptrs++] = p_act;
	mvrlu_assert(self->free_ptrs.num_ptrs < MVRLU_MAX_FREE_PTRS);
}
EXPORT_SYMBOL(mvrlu_free);

void mvrlu_reader_lock(mvrlu_thread_struct_t *self)
{
	/* Secure a large enough log space */
	if (unlikely(self->log.need_reclaim))
		log_reclaim(&self->log);
	/* - capacity water mark */
	if (unlikely(log_used(&self->log) >= MVRLU_LOG_HIGH_MARK)) {
		do {
			log_reclaim_force(&self->log);
			stat_thread_inc(self, n_high_mark_block);
		} while (log_used(&self->log) >= MVRLU_LOG_HIGH_MARK);
	}

	/* Object data writes should not be reordered with metadata writes. */
	smp_wmb_tso();

	/* Get it started */
	smp_faa(&(self->run_cnt), 1);
	self->num_act_obj = 0;
	self->num_deref = 0;
	self->local_clk = get_clock_relaxed();

	/* Get the latest view */
	smp_rmb();

	stat_thread_inc(self, n_starts);
	mvrlu_assert(self->log.cur_wrt_set == NULL);
	mvrlu_assert(self->free_ptrs.num_ptrs == 0);
}
EXPORT_SYMBOL(mvrlu_reader_lock);

void mvrlu_reader_unlock(mvrlu_thread_struct_t *self)
{
	/* Object data writes should not be reordered with metadata writes. */
	smp_wmb_tso();

	mvrlu_assert(self->run_cnt & 0x1);
	self->run_cnt++;

	/* If dereference takes too much overhead, reclaim log */
	/* - dereference water mark */
	if (self->num_deref && self->num_act_obj > MVRLU_DEREF_MIN_ACT_OBJ &&
	    self->num_act_obj < (MVRLU_DEREF_MARK * self->num_deref)) {
		wakeup_qp_thread_for_reclaim();
	}

	/* If write or log reclaim is needed, we need write memory
	 * barrier to avoid reordering of metadata updates. */
	if (self->is_write_detected || self->log.need_reclaim) {
		if (self->is_write_detected) {
			self->is_write_detected = 0;
			log_commit(&self->log, &self->free_ptrs,
				   self->local_clk);
		}

		if (unlikely(self->log.need_reclaim))
			log_reclaim(&self->log);

		if (unlikely(log_used(&self->log) >= MVRLU_LOG_LOW_MARK)) {
			if (wakeup_qp_thread_for_reclaim()) {
				stat_thread_inc(self, n_low_mark_wakeup);
			}
		}
		smp_wmb();
	}

	stat_thread_inc(self, n_finish);
	mvrlu_assert(self->log.cur_wrt_set == NULL);
	mvrlu_assert(self->free_ptrs.num_ptrs == 0);
}
EXPORT_SYMBOL(mvrlu_reader_unlock);

void mvrlu_abort(mvrlu_thread_struct_t *self)
{
	/* Object data writes should not be reordered with metadata writes. */
	smp_wmb_tso();

	mvrlu_assert(self->run_cnt & 0x1);
	self->run_cnt++;

	if (self->log.cur_wrt_set) {
		log_abort(&self->log, &self->free_ptrs);
		self->is_write_detected = 0;
	}

	if (unlikely(self->log.need_reclaim))
		log_reclaim(&self->log);

	/* Prepare next mvrlu_reader_lock() by performing memory barrier. */
	smp_mb();

	stat_thread_inc(self, n_aborts);
	mvrlu_assert(self->log.cur_wrt_set == NULL);
	mvrlu_assert(self->free_ptrs.num_ptrs == 0);
}
EXPORT_SYMBOL(mvrlu_abort);

void *mvrlu_deref(mvrlu_thread_struct_t *self, void *obj)
{
	volatile void *p_act, *p_copy;
	mvrlu_cpy_hdr_struct_t *chs;
	unsigned long wrt_clk;
	unsigned long qp_clk2;

	if (unlikely(!obj))
		return NULL;

	p_act = get_act_obj(obj);
	mvrlu_assert(p_act && vobj_to_obj_hdr(p_act)->type == TYPE_ACTUAL);
	self->num_act_obj++;

	p_copy = vobj_to_obj_hdr(p_act)->p_copy;
	if (unlikely(p_copy)) {
		qp_clk2 = self->log.qp_clk2;
		self->num_deref++;
		do {
			chs = vobj_to_chs(p_copy);
			wrt_clk = get_wrt_clk(chs);
			if (lte_clock(wrt_clk, self->local_clk))
				return (void *)p_copy;

			if (unlikely(lte_clock(chs->cpy_hdr.wrt_clk_next,
					       qp_clk2)))
				break;
			p_copy = chs->obj_hdr.p_copy;
		} while (p_copy);
	}
	return (void *)p_act;
}
EXPORT_SYMBOL(mvrlu_deref);

int _mvrlu_try_lock(mvrlu_thread_struct_t *self, void **pp_obj, size_t size)
{
	volatile void *p_act, *p_lock, *p_old_copy, *p_new_copy;
	mvrlu_act_hdr_struct_t *ahs;
	mvrlu_cpy_hdr_struct_t *chs;
	void *obj;
	int bogus_allocated;

	obj = *pp_obj;
	mvrlu_warning(obj != NULL);

	p_act = get_act_obj(obj);
	mvrlu_assert(p_act && vobj_to_obj_hdr(p_act)->type == TYPE_ACTUAL);

	/* If an object is already locked, it cannot lock again
	 * except when a lock is locked again by the same thread. */
	ahs = vobj_to_ahs(p_act);
	p_lock = ahs->act_hdr.p_lock;
	if (unlikely(p_lock)) {
#ifdef MVRLU_NESTED_LOCKING
		if (self == chs_to_thread(vobj_to_chs(p_lock))) {
			/* If the lock is acquired by the same thread,
			 * allow to lock again according to the original
			 * RLU semantics.
			 *
			 * WARNING: We do not promote immutable try_lock_const()
			 * to mutable try_lock_const().
			 */
			mvrlu_warning(size <= ahs->obj_hdr.obj_size);
			*pp_obj = (void *)p_lock;
			return 1;
		}
#endif
		return 0;
	}

	/* To maintain a linear version history, we should allow
	 * lock acquisition only when the local version of a thread
	 * is greater or equal to the writer version of an object.
	 * Otherwise it allows inconsistent, mixed, views
	 * of the local version and the writer version.
	 * That is because acquiring a lock fundamentally means
	 * advancing the version. */
	p_old_copy = ahs->obj_hdr.p_copy;
	if (p_old_copy) {
		chs = vobj_to_chs(p_old_copy);
		/* It guarantees that clock gap between two versions of
		 * an object is greater than 2x ORDO_BOUNDARY. */
		if (!lte_clock(get_wrt_clk(chs), self->local_clk))
			return 0;
	}

	/* Secure log space and initialize a header */
	chs = log_append_begin(&self->log, p_act, size, &bogus_allocated);
	p_new_copy = (volatile void *)chs->obj_hdr.obj;

	/* Try lock */
	if (!try_lock_obj(ahs, p_old_copy, p_new_copy)) {
		log_append_abort(&self->log, chs);
		return 0;
	}

	/* Duplicate the copy */
	if (!p_old_copy)
		memcpy((void *)p_new_copy, (void *)p_act, size);
	else
		memcpy((void *)p_new_copy, (void *)p_old_copy, size);
	log_append_end(&self->log, chs, bogus_allocated);

	/* Succeed in locking */
	if (self->is_write_detected == 0)
		self->is_write_detected = 1;
	*pp_obj = (void *)p_new_copy;

	mvrlu_assert(ahs->act_hdr.p_lock);
	return 1;
}
EXPORT_SYMBOL(_mvrlu_try_lock);

int _mvrlu_try_lock_const(mvrlu_thread_struct_t *self, void *obj, size_t size)
{
	/* Try_lock_const is nothing but a try lock with size zero
	 * so we can omit copy from/to p_act.
	 *
	 * NOTE: obj is not updated after the call (not void ** but void *) */
	return _mvrlu_try_lock(self, &obj, 0);
}
EXPORT_SYMBOL(_mvrlu_try_lock_const);

int mvrlu_cmp_ptrs(void *obj1, void *obj2)
{
	if (likely(obj1 != NULL))
		obj1 = get_act_obj(obj1);
	if (likely(obj2 != NULL))
		obj2 = get_act_obj(obj2);
	return obj1 == obj2;
}
EXPORT_SYMBOL(mvrlu_cmp_ptrs);

void _mvrlu_assign_pointer(void **p_ptr, void *obj)
{
	if (likely(obj != NULL))
		obj = get_act_obj(obj);
	*p_ptr = obj;
}
EXPORT_SYMBOL(_mvrlu_assign_pointer);

void mvrlu_flush_log(mvrlu_thread_struct_t *self)
{
	while (self->log.head_cnt != self->log.tail_cnt) {
		log_reclaim_force(&self->log);
	}
#ifdef MVRLU_ENABLE_STATS
	stat_reset(&(self)->stat);
#endif
}
EXPORT_SYMBOL(mvrlu_flush_log);

void mvrlu_print_stats(void)
{
	printf("=================================================\n");
	printf("MV-RLU configuration:\n");
	printf("-------------------------------------------------\n");
	print_config();
	printf("-------------------------------------------------\n");

	/* It should be called after mvrlu_finish(). */
#ifdef MVRLU_ENABLE_STATS
	printf("MV-RLU statistics:\n");
	printf("-------------------------------------------------\n");
	stat_print_cnt(&g_stat);
	printf("-------------------------------------------------\n");
#endif
}

static void print_config(void)
{
	printf(MVRLU_COLOR_GREEN
#ifdef MVRLU_ORDO_TIMESTAMPING
	       "  MVRLU_ORDO_TIMESTAMPING = 1\n"
#else
	       "  MVRLU_ORDO_TIMESTAMPING = 0\n"
#endif
	       MVRLU_COLOR_RESET);
	printf(MVRLU_COLOR_GREEN "  MVRLU_LOG_SIZE = %ld\n" MVRLU_COLOR_RESET,
	       MVRLU_LOG_SIZE);
	printf(MVRLU_COLOR_GREEN
	       "  MVRLU_LOG_LOW_MARK = %ld\n" MVRLU_COLOR_RESET,
	       MVRLU_LOG_LOW_MARK);
	printf(MVRLU_COLOR_GREEN
	       "  MVRLU_LOG_HIGH_MARK = %ld\n" MVRLU_COLOR_RESET,
	       MVRLU_LOG_HIGH_MARK);
#ifdef MVRLU_ENABLE_ASSERT
	printf(MVRLU_COLOR_RED "  MVRLU_ENABLE_ASSERT is on.          "
			       "DO NOT USE FOR BENCHMARK!\n" MVRLU_COLOR_RESET);
#endif
#ifdef MVRLU_ENABLE_FREE_POISIONING
	printf(MVRLU_COLOR_RED "  MVRLU_ENABLE_FREE_POISIONING is on. "
			       "DO NOT USE FOR BENCHMARK!\n" MVRLU_COLOR_RESET);
#endif
#ifdef MVRLU_ENABLE_STATS
	printf(MVRLU_COLOR_RED "  MVRLU_ENABLE_STATS is on.       "
			       "DO NOT USE FOR BENCHMARK!\n" MVRLU_COLOR_RESET);
#endif
#ifdef MVRLU_TIME_MEASUREMENT
	printf(MVRLU_COLOR_RED "  MVRLU_TIME_MEASUREMENT is on.       "
			       "DO NOT USE FOR BENCHMARK!\n" MVRLU_COLOR_RESET);
#endif
#if defined(MVRLU_ENABLE_TRACE_0) || defined(MVRLU_ENABLE_TRACE_1) ||          \
	defined(MVRLU_ENABLE_TRACE_2) || defined(MVRLU_ENABLE_TRACE_3)
	printf(MVRLU_COLOR_MAGENTA
	       "  MVRLU_ENABLE_TRACE_*  is on.        "
	       "IT MAY AFFECT BENCHMARK RESULTS!\n" MVRLU_COLOR_RESET);
#endif
}
EXPORT_SYMBOL(mvrlu_print_stats);
