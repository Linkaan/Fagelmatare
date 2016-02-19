/*
 *  lstack.h
 *    This is a small library that provides a lock-free stack using C11's new
 *    stdatomic.h features. It's ABA aware and requires double-wide CAS.
 *    Copyright (C) 2015 Linus Styrén
 *****************************************************************************
 *  This file is part of Fågelmataren:
 *    https://github.com/Linkaan/Fagelmatare/
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *****************************************************************************
 */

#ifndef LSTACK_H
#define LSTACK_H

#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>

struct lstack_node {
    void *value;
    struct lstack_node *next;
};

struct lstack_head {
    uintptr_t aba;
    struct lstack_node *node;
};

typedef struct {
    struct lstack_node *node_buffer;
    _Atomic struct lstack_head head, free;
    _Atomic size_t size;
} lstack_t;

static inline size_t lstack_size(lstack_t *lstack)
{
    return atomic_load(&lstack->size);
}

static inline void lstack_free(lstack_t *lstack)
{
    free(lstack->node_buffer);
}

int   lstack_init(lstack_t *lstack, size_t max_size);
int   lstack_push(lstack_t *lstack, void *value);
void *lstack_pop(lstack_t *lstack);

#endif
