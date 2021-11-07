// SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
// SPDX-License-Identifier: Apache-2.0
#ifndef _PORT_KERNEL_H
#define _PORT_KERNEL_H

#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/timekeeping.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#define printf(...) pr_info(__VA_ARGS__)
#define printf_err(...) pr_err(__VA_ARGS__)
#define fprintf(arg, ...) pr_err(__VA_ARGS__)

/*
 * Log region
 */
static unsigned long g_size __read_mostly;

static inline int port_log_region_init(unsigned long size, unsigned long num)
{
	g_size = size;
	return 0;
}

static inline void port_log_region_destroy(void)
{
	/* do nothing */
}

static inline void *port_alloc_log_mem(void)
{
	return vmalloc(g_size);
}

static inline void port_free_log_mem(void *addr)
{
	vfree(addr);
}

static inline int port_addr_in_log_region(void *addr__)
{
	unsigned long addr = (unsigned long)addr__;
	return addr >= VMALLOC_START && addr < VMALLOC_END;
}

/*
 * Memory allocation
 */

#define PORT_DEFAULT_ALLOC_FLAG GFP_KERNEL

static inline void *port_alloc_x(size_t size, unsigned int flags)
{
	return kmalloc(size, flags);
}

static inline void port_free(void *ptr)
{
	kfree(ptr);
}

/*
 * Synchronization
 */

static inline void port_cpu_relax_and_yield(void)
{
	if (need_resched())
		cond_resched();
	cpu_relax();
}

static inline void port_spin_init(spinlock_t *lock)
{
	spin_lock_init(lock);
}

static inline void port_spin_destroy(spinlock_t *lock)
{
	/* do nothing */
}

static inline void port_spin_lock(spinlock_t *lock)
{
	spin_lock(lock);
}

static inline int port_spin_trylock(spinlock_t *lock)
{
	return spin_trylock(lock);
}

static inline void port_spin_unlock(spinlock_t *lock)
{
	spin_unlock(lock);
}

static inline int port_mutex_init(struct mutex *mutex)
{
	mutex_init(mutex);
	return 0;
}

static inline int port_mutex_destroy(struct mutex *mutex)
{
	mutex_destroy(mutex);
	return 0;
}

static inline int port_mutex_lock(struct mutex *mutex)
{
	mutex_lock(mutex);
	return 0;
}

static inline int port_mutex_unlock(struct mutex *mutex)
{
	mutex_unlock(mutex);
	return 0;
}

static inline void port_cond_init(struct completion *cond)
{
	init_completion(cond);
}

static inline void port_cond_destroy(struct completion *cond)
{
	/* do nothing */
}

/*
 * Thread
 */

static int port_create_thread(const char *name, struct task_struct **t,
			      int (*fn)(void *), void *arg,
			      struct completion *completion)
{
	struct task_struct *temp;

	temp = kthread_create(fn, arg, name);
	if (temp != NULL) {
		init_completion(completion);
		wake_up_process(temp);
		*t = temp;
		return 0;
	}
	return -EAGAIN;
}

static void port_finish_thread(struct completion *completion)
{
	complete(completion);
}

static void port_wait_for_finish(void *x, struct completion *completion)
{
	wait_for_completion(completion);
}

static inline void port_initiate_wakeup(struct mutex *mutex,
					struct completion *cond)
{
	complete(cond);
	set_current_state(TASK_RUNNING);
}

static inline void port_initiate_nap(struct mutex *mutex,
				     struct completion *cond,
				     unsigned long usecs)
{
	reinit_completion(cond);
	wait_for_completion_interruptible_timeout(cond,
						  usecs_to_jiffies(usecs));
}
#endif /* _PORT_KERNEL_H */
