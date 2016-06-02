/*
 * Copyright (c) 2011-2015 Richard Braun.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Upstream site with license notes :
 * http://git.sceen.net/rbraun/librbraun.git/
 */

#ifndef _RDXTREE_I_H
#define _RDXTREE_I_H

/*
 * Radix tree.
 */
struct rdxtree {
    unsigned int height;
    void *root;
};

/*
 * Radix tree iterator.
 *
 * The node member refers to the node containing the current pointer, if any.
 * The key member refers to the current pointer, and is valid if and only if
 * rdxtree_walk() has been called at least once on the iterator.
 */
struct rdxtree_iter {
    void *node;
    rdxtree_key_t key;
};

/*
 * Initialize an iterator.
 */
static inline void
rdxtree_iter_init(struct rdxtree_iter *iter)
{
    iter->node = NULL;
    iter->key = (rdxtree_key_t)-1;
}

int rdxtree_insert_common(struct rdxtree *tree, rdxtree_key_t key,
                          void *ptr, void ***slotp);

int rdxtree_insert_alloc_common(struct rdxtree *tree, void *ptr,
                                rdxtree_key_t *keyp, void ***slotp);

void * rdxtree_lookup_common(const struct rdxtree *tree, rdxtree_key_t key,
                             int get_slot);

void * rdxtree_walk(struct rdxtree *tree, struct rdxtree_iter *iter);

#endif /* _RDXTREE_I_H */
