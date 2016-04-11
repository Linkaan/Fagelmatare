/*
 *  lstack.h
 *    This is a small library that provides a lock-free stack using C11's new
 *    stdatomic.h features. It's ABA aware and requires double-wide CAS.
 *****************************************************************************
 *  This file is part of Fågelmataren, an advanced bird feeder equipped with
 *  many peripherals. See <https://github.com/Linkaan/Fagelmatare>
 *  Copyright (C) 2015-2016 Linus Styrén
 *
 *  Fågelmataren is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the Licence, or
 *  (at your option) any later version.
 *
 *  Fågelmataren is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with Fågelmataren.  If not, see <http://www.gnu.org/licenses/>.
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
