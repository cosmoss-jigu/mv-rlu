// SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
// SPDX-License-Identifier: Apache-2.0
#ifndef _MVRLU_H
#define _MVRLU_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __KERNEL__
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#else
#include <linux/compiler.h>
#include <linux/sched.h>
#endif /* __KERNEL__ */

/*
 * Forward declaration of mvrlu_thread_struct_t
 */
typedef struct mvrlu_thread_struct mvrlu_thread_struct_t;

/*
 * MV-RLU API
 */
int mvrlu_init(void);
void mvrlu_finish(void);
void mvrlu_print_stats(void);

mvrlu_thread_struct_t *mvrlu_thread_alloc(void);
void mvrlu_thread_free(mvrlu_thread_struct_t *self);

void mvrlu_thread_init(mvrlu_thread_struct_t *self);
void mvrlu_thread_finish(mvrlu_thread_struct_t *self);

void *mvrlu_alloc(size_t size);
void *mvrlu_alloc_x(size_t size, unsigned int flags);
void mvrlu_free(mvrlu_thread_struct_t *self, void *p_obj);

void mvrlu_reader_lock(mvrlu_thread_struct_t *self);
void mvrlu_reader_unlock(mvrlu_thread_struct_t *self);
void mvrlu_abort(mvrlu_thread_struct_t *self);

int _mvrlu_try_lock(mvrlu_thread_struct_t *self, void **p_p_obj, size_t size);
int _mvrlu_try_lock_const(mvrlu_thread_struct_t *self, void *obj, size_t size);

int mvrlu_cmp_ptrs(void *p_obj_1, void *p_obj_2);

void _mvrlu_assign_pointer(void **p_ptr, void *p_obj);
void *mvrlu_deref(mvrlu_thread_struct_t *self, void *p_obj);

void mvrlu_dump_stack(void);
void mvrlu_attach_gdb(void);

void mvrlu_flush_log(mvrlu_thread_struct_t *self);

#define mvrlu_try_lock(self, p_p_obj)                                          \
	_mvrlu_try_lock(self, (void **)p_p_obj, sizeof(**p_p_obj))
#define mvrlu_try_lock_const(self, obj)                                        \
	_mvrlu_try_lock_const(self, obj, sizeof(*obj))
#define mvrlu_assign_ptr(self, p_ptr, p_obj)                                   \
	_mvrlu_assign_pointer((void **)p_ptr, p_obj)

/*
 * MV-RLU API for kernel
 */
#ifdef __KERNEL__
static inline void kmvrlu_print_stats(void)
{
	return mvrlu_print_stats();
}

static inline mvrlu_thread_struct_t *kmvrlu_thread_alloc(void)
{
	return mvrlu_thread_alloc();
}

static inline void kmvrlu_thread_free(void)
{
	mvrlu_thread_free(current->mvrlu_self);
}

static inline void kmvrlu_thread_init(void)
{
	mvrlu_thread_init(current->mvrlu_self);
}

static inline void kmvrlu_thread_finish(void)
{
	mvrlu_thread_finish(current->mvrlu_self);
}

static inline void *kmvrlu_alloc(size_t size)
{
	return mvrlu_alloc(size);
}

static inline void kmvrlu_free(void *p_obj)
{
	return mvrlu_free(current->mvrlu_self, p_obj);
}

static inline void kmvrlu_reader_lock(void)
{
	return mvrlu_reader_lock(current->mvrlu_self);
}

static inline void kmvrlu_reader_unlock(void)
{
	mvrlu_reader_unlock(current->mvrlu_self);
}

static inline void kmvrlu_abort(void)
{
	mvrlu_abort(current->mvrlu_self);
}

static inline int kmvrlu_cmp_ptrs(void *p_obj_1, void *p_obj_2)
{
	return mvrlu_cmp_ptrs(p_obj_1, p_obj_2);
}

static inline void *kmvrlu_deref(void *p_obj)
{
	return mvrlu_deref(current->mvrlu_self, p_obj);
}

static inline void kmvrlu_flush_log(void)
{
	mvrlu_flush_log(current->mvrlu_self);
}

#define kmvrlu_try_lock(p_p_obj)                                               \
	_mvrlu_try_lock(current->mvrlu_self, (void **)p_p_obj,                 \
			sizeof(**p_p_obj))
#define kmvrlu_try_lock_const(obj)                                             \
	_mvrlu_try_lock_const(current->mvrlu_self, obj, sizeof(*obj))
#define kmvrlu_assign_ptr(p_ptr, p_obj)                                        \
	_mvrlu_assign_pointer((void **)p_ptr, p_obj)

#endif /* __KERNEL__ */

/*
 * Compatibility wrapper for RLU
 */
typedef mvrlu_thread_struct_t rlu_thread_data_t;

#define RLU_INIT() mvrlu_init()
#define RLU_FINISH() mvrlu_finish()
#define RLU_PRINT_STATS() mvrlu_print_stats()

#define RLU_THREAD_ALLOC() mvrlu_thread_alloc()
#define RLU_THREAD_FREE(self) mvrlu_thread_free(self)

#define RLU_THREAD_INIT(self) mvrlu_thread_init(self)
#define RLU_THREAD_FINISH(self) mvrlu_thread_finish(self)

#define RLU_READER_LOCK(self) mvrlu_reader_lock(self)
#define RLU_READER_UNLOCK(self) mvrlu_reader_unlock(self)

#define RLU_ALLOC(size) mvrlu_alloc(size)
#define RLU_FREE(self, p_obj) mvrlu_free(self, p_obj)

#define RLU_TRY_LOCK(self, p_p_obj) mvrlu_try_lock(self, p_p_obj)
#define RLU_TRY_LOCK_CONST(self, obj) mvrlu_try_lock_const(self, obj)
#define RLU_ABORT(self) mvrlu_abort(self)

#define RLU_IS_SAME_PTRS(p_obj_1, p_obj_2) mvrlu_cmp_ptrs(p_obj_1, p_obj_2)
#define RLU_ASSIGN_PTR(self, p_ptr, p_obj) mvrlu_assign_ptr(self, p_ptr, p_obj)

#define RLU_DEREF(self, p_obj) mvrlu_deref(self, p_obj)

#ifdef __cplusplus
}
#endif

#endif /* _MVRLU_H */
