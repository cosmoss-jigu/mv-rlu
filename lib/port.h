// SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
// SPDX-License-Identifier: Apache-2.0
#ifndef _PORT_H
#define _PORT_H

#include "arch.h"

#ifdef __KERNEL__
#include "port-kernel.h"
#else
#include "port-user.h"
#endif

static inline void *port_alloc(size_t size)
{
	return port_alloc_x(size, PORT_DEFAULT_ALLOC_FLAG);
}
#endif /* _PORT_H */
