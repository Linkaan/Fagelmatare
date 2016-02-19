/*
 *  lstack.c
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

#include <stdlib.h>
#include <errno.h>
#include <lstack.h>

int lstack_init(lstack_t *lstack, size_t max_size)
{
    lstack->head.aba = ATOMIC_VAR_INIT(0);
    lstack->head.node = ATOMIC_VAR_INIT(NULL);
    lstack->size = ATOMIC_VAR_INIT(0);

    /* Pre-allocate all nodes. */
    lstack->node_buffer = malloc(max_size * sizeof(struct lstack_node));
    if (lstack->node_buffer == NULL)
        return ENOMEM;
    for (size_t i = 0; i < max_size - 1; i++)
        lstack->node_buffer[i].next = lstack->node_buffer + i + 1;
    lstack->node_buffer[max_size - 1].next = NULL;
    lstack->free.aba = ATOMIC_VAR_INIT(0);
    lstack->free.node = ATOMIC_VAR_INIT(lstack->node_buffer);
    return 0;
}

static struct lstack_node *pop(_Atomic struct lstack_head *head)
{
    struct lstack_head next, orig = atomic_load(head);
    do {
        if (orig.node == NULL)
            return NULL;
        next.aba = orig.aba + 1;
        next.node = orig.node->next;
    } while (!atomic_compare_exchange_weak(head, &orig, next));
    return orig.node;
}

static void push(_Atomic struct lstack_head *head, struct lstack_node *node)
{
    struct lstack_head next, orig = atomic_load(head);
    do {
        node->next = orig.node;
        next.aba = orig.aba + 1;
        next.node = node;
    } while (!atomic_compare_exchange_weak(head, &orig, next));
}

int lstack_push(lstack_t *lstack, void *value)
{
    struct lstack_node *node = pop(&lstack->free);
    if (node == NULL)
        return ENOMEM;
    node->value = value;
    push(&lstack->head, node);
    atomic_fetch_add(&lstack->size, 1);
    return 0;
}

void *lstack_pop(lstack_t *lstack)
{
    struct lstack_node *node = pop(&lstack->head);
    if (node == NULL)
        return NULL;
    atomic_fetch_sub(&lstack->size, 1);
    void *value = node->value;
    push(&lstack->free, node);
    return value;
}
