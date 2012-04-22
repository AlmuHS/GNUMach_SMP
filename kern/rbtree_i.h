/*
 * Copyright (c) 2010, 2011 Richard Braun.
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
 */

#ifndef _KERN_RBTREE_I_H
#define _KERN_RBTREE_I_H

#include <kern/assert.h>

/*
 * Red-black node structure.
 *
 * To reduce the number of branches and the instruction cache footprint,
 * the left and right child pointers are stored in an array, and the symmetry
 * of most tree operations is exploited by using left/right variables when
 * referring to children.
 *
 * In addition, this implementation assumes that all nodes are 4-byte aligned,
 * so that the least significant bit of the parent member can be used to store
 * the color of the node. This is true for all modern 32 and 64 bits
 * architectures, as long as the nodes aren't embedded in structures with
 * special alignment constraints such as member packing.
 */
struct rbtree_node {
    unsigned long parent;
    struct rbtree_node *children[2];
};

/*
 * Red-black tree structure.
 */
struct rbtree {
    struct rbtree_node *root;
};

/*
 * Masks applied on the parent member of a node to obtain either the
 * color or the parent address.
 */
#define RBTREE_COLOR_MASK   0x1UL
#define RBTREE_PARENT_MASK  (~0x3UL)

/*
 * Node colors.
 */
#define RBTREE_COLOR_RED    0
#define RBTREE_COLOR_BLACK  1

/*
 * Masks applied on slots to obtain either the child index or the parent
 * address.
 */
#define RBTREE_SLOT_INDEX_MASK  0x1UL
#define RBTREE_SLOT_PARENT_MASK (~RBTREE_SLOT_INDEX_MASK)

/*
 * Return true if the given pointer is suitably aligned.
 */
static inline int rbtree_check_alignment(const struct rbtree_node *node)
{
    return ((unsigned long)node & (~RBTREE_PARENT_MASK)) == 0;
}

/*
 * Return true if the given index is a valid child index.
 */
static inline int rbtree_check_index(int index)
{
    return index == (index & 1);
}

/*
 * Convert the result of a comparison into an index in the children array
 * (0 or 1).
 *
 * This function is mostly used when looking up a node.
 */
static inline int rbtree_d2i(int diff)
{
    return !(diff <= 0);
}

/*
 * Return the parent of a node.
 */
static inline struct rbtree_node * rbtree_parent(const struct rbtree_node *node)
{
    return (struct rbtree_node *)(node->parent & RBTREE_PARENT_MASK);
}

/*
 * Translate an insertion point into a slot.
 */
static inline unsigned long rbtree_slot(struct rbtree_node *parent, int index)
{
    assert(rbtree_check_alignment(parent));
    assert(rbtree_check_index(index));
    return (unsigned long)parent | index;
}

/*
 * Extract the parent address from a slot.
 */
static inline struct rbtree_node * rbtree_slot_parent(unsigned long slot)
{
    return (struct rbtree_node *)(slot & RBTREE_SLOT_PARENT_MASK);
}

/*
 * Extract the index from a slot.
 */
static inline int rbtree_slot_index(unsigned long slot)
{
    return slot & RBTREE_SLOT_INDEX_MASK;
}

/*
 * Insert a node in a tree, rebalancing it if necessary.
 *
 * The index parameter is the index in the children array of the parent where
 * the new node is to be inserted. It is ignored if the parent is null.
 *
 * This function is intended to be used by the rbtree_insert() macro only.
 */
void rbtree_insert_rebalance(struct rbtree *tree, struct rbtree_node *parent,
                             int index, struct rbtree_node *node);

/*
 * Return the previous or next node relative to a location in a tree.
 *
 * The parent and index parameters define the location, which can be empty.
 * The direction parameter is either RBTREE_LEFT (to obtain the previous
 * node) or RBTREE_RIGHT (to obtain the next one).
 */
struct rbtree_node * rbtree_nearest(struct rbtree_node *parent, int index,
                                    int direction);

/*
 * Return the first or last node of a tree.
 *
 * The direction parameter is either RBTREE_LEFT (to obtain the first node)
 * or RBTREE_RIGHT (to obtain the last one).
 */
struct rbtree_node * rbtree_firstlast(const struct rbtree *tree, int direction);

/*
 * Return the node next to, or previous to the given node.
 *
 * The direction parameter is either RBTREE_LEFT (to obtain the previous node)
 * or RBTREE_RIGHT (to obtain the next one).
 */
struct rbtree_node * rbtree_walk(struct rbtree_node *node, int direction);

/*
 * Return the left-most deepest node of a tree, which is the starting point of
 * the postorder traversal performed by rbtree_for_each_remove().
 */
struct rbtree_node * rbtree_postwalk_deepest(const struct rbtree *tree);

/*
 * Unlink a node from its tree and return the next (right) node in postorder.
 */
struct rbtree_node * rbtree_postwalk_unlink(struct rbtree_node *node);

#endif /* _KERN_RBTREE_I_H */
