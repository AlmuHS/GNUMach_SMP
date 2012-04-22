/*
 * Copyright (c) 2010, 2012 Richard Braun.
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

#include <kern/assert.h>
#include <kern/rbtree.h>
#include <kern/rbtree_i.h>
#include <sys/types.h>

#define unlikely(expr) __builtin_expect(!!(expr), 0)

/*
 * Return the index of a node in the children array of its parent.
 *
 * The parent parameter must not be null, and must be the parent of the
 * given node.
 */
static inline int rbtree_index(const struct rbtree_node *node,
                               const struct rbtree_node *parent)
{
    assert(parent != NULL);
    assert((node == NULL) || (rbtree_parent(node) == parent));

    if (parent->children[RBTREE_LEFT] == node)
        return RBTREE_LEFT;

    assert(parent->children[RBTREE_RIGHT] == node);

    return RBTREE_RIGHT;
}

/*
 * Return the color of a node.
 */
static inline int rbtree_color(const struct rbtree_node *node)
{
    return node->parent & RBTREE_COLOR_MASK;
}

/*
 * Return true if the node is red.
 */
static inline int rbtree_is_red(const struct rbtree_node *node)
{
    return rbtree_color(node) == RBTREE_COLOR_RED;
}

/*
 * Return true if the node is black.
 */
static inline int rbtree_is_black(const struct rbtree_node *node)
{
    return rbtree_color(node) == RBTREE_COLOR_BLACK;
}

/*
 * Set the parent of a node, retaining its current color.
 */
static inline void rbtree_set_parent(struct rbtree_node *node,
                                     struct rbtree_node *parent)
{
    assert(rbtree_check_alignment(node));
    assert(rbtree_check_alignment(parent));

    node->parent = (unsigned long)parent | (node->parent & RBTREE_COLOR_MASK);
}

/*
 * Set the color of a node, retaining its current parent.
 */
static inline void rbtree_set_color(struct rbtree_node *node, int color)
{
    assert((color & ~RBTREE_COLOR_MASK) == 0);
    node->parent = (node->parent & RBTREE_PARENT_MASK) | color;
}

/*
 * Set the color of a node to red, retaining its current parent.
 */
static inline void rbtree_set_red(struct rbtree_node *node)
{
    rbtree_set_color(node, RBTREE_COLOR_RED);
}

/*
 * Set the color of a node to black, retaining its current parent.
 */
static inline void rbtree_set_black(struct rbtree_node *node)
{
    rbtree_set_color(node, RBTREE_COLOR_BLACK);
}

/*
 * Perform a tree rotation, rooted at the given node.
 *
 * The direction parameter defines the rotation direction and is either
 * RBTREE_LEFT or RBTREE_RIGHT.
 */
static void rbtree_rotate(struct rbtree *tree, struct rbtree_node *node,
                          int direction)
{
    struct rbtree_node *parent, *rnode;
    int left, right;

    left = direction;
    right = 1 - left;
    parent = rbtree_parent(node);
    rnode = node->children[right];

    node->children[right] = rnode->children[left];

    if (rnode->children[left] != NULL)
        rbtree_set_parent(rnode->children[left], node);

    rnode->children[left] = node;
    rbtree_set_parent(rnode, parent);

    if (unlikely(parent == NULL))
        tree->root = rnode;
    else
        parent->children[rbtree_index(node, parent)] = rnode;

    rbtree_set_parent(node, rnode);
}

void rbtree_insert_rebalance(struct rbtree *tree, struct rbtree_node *parent,
                             int index, struct rbtree_node *node)
{
    struct rbtree_node *grand_parent, *uncle, *tmp;
    int left, right;

    assert(rbtree_check_alignment(parent));
    assert(rbtree_check_alignment(node));

    node->parent = (unsigned long)parent | RBTREE_COLOR_RED;
    node->children[RBTREE_LEFT] = NULL;
    node->children[RBTREE_RIGHT] = NULL;

    if (unlikely(parent == NULL))
        tree->root = node;
    else
        parent->children[index] = node;

    for (;;) {
        if (parent == NULL) {
            rbtree_set_black(node);
            break;
        }

        if (rbtree_is_black(parent))
            break;

        grand_parent = rbtree_parent(parent);
        assert(grand_parent != NULL);

        left = rbtree_index(parent, grand_parent);
        right = 1 - left;

        uncle = grand_parent->children[right];

        /*
         * Uncle is red. Flip colors and repeat at grand parent.
         */
        if ((uncle != NULL) && rbtree_is_red(uncle)) {
            rbtree_set_black(uncle);
            rbtree_set_black(parent);
            rbtree_set_red(grand_parent);
            node = grand_parent;
            parent = rbtree_parent(node);
            continue;
        }

        /*
         * Node is the right child of its parent. Rotate left at parent.
         */
        if (parent->children[right] == node) {
            rbtree_rotate(tree, parent, left);
            tmp = node;
            node = parent;
            parent = tmp;
        }

        /*
         * Node is the left child of its parent. Handle colors, rotate right
         * at grand parent, and leave.
         */
        rbtree_set_black(parent);
        rbtree_set_red(grand_parent);
        rbtree_rotate(tree, grand_parent, right);
        break;
    }

    assert(rbtree_is_black(tree->root));
}

void rbtree_remove(struct rbtree *tree, struct rbtree_node *node)
{
    struct rbtree_node *child, *parent, *brother;
    int color, left, right;

    if (node->children[RBTREE_LEFT] == NULL)
        child = node->children[RBTREE_RIGHT];
    else if (node->children[RBTREE_RIGHT] == NULL)
        child = node->children[RBTREE_LEFT];
    else {
        struct rbtree_node *successor;

        /*
         * Two-children case: replace the node with its successor.
         */

        successor = node->children[RBTREE_RIGHT];

        while (successor->children[RBTREE_LEFT] != NULL)
            successor = successor->children[RBTREE_LEFT];

        color = rbtree_color(successor);
        child = successor->children[RBTREE_RIGHT];
        parent = rbtree_parent(node);

        if (unlikely(parent == NULL))
            tree->root = successor;
        else
            parent->children[rbtree_index(node, parent)] = successor;

        parent = rbtree_parent(successor);

        /*
         * Set parent directly to keep the original color.
         */
        successor->parent = node->parent;
        successor->children[RBTREE_LEFT] = node->children[RBTREE_LEFT];
        rbtree_set_parent(successor->children[RBTREE_LEFT], successor);

        if (node == parent)
            parent = successor;
        else {
            successor->children[RBTREE_RIGHT] = node->children[RBTREE_RIGHT];
            rbtree_set_parent(successor->children[RBTREE_RIGHT], successor);
            parent->children[RBTREE_LEFT] = child;

            if (child != NULL)
                rbtree_set_parent(child, parent);
        }

        goto update_color;
    }

    /*
     * Node has at most one child.
     */

    color = rbtree_color(node);
    parent = rbtree_parent(node);

    if (child != NULL)
        rbtree_set_parent(child, parent);

    if (unlikely(parent == NULL))
        tree->root = child;
    else
        parent->children[rbtree_index(node, parent)] = child;

    /*
     * The node has been removed, update the colors. The child pointer can
     * be null, in which case it is considered a black leaf.
     */
update_color:
    if (color == RBTREE_COLOR_RED)
        return;

    for (;;) {
        if ((child != NULL) && rbtree_is_red(child)) {
            rbtree_set_black(child);
            break;
        }

        if (parent == NULL)
            break;

        left = rbtree_index(child, parent);
        right = 1 - left;

        brother = parent->children[right];

        /*
         * Brother is red. Recolor and rotate left at parent so that brother
         * becomes black.
         */
        if (rbtree_is_red(brother)) {
            rbtree_set_black(brother);
            rbtree_set_red(parent);
            rbtree_rotate(tree, parent, left);
            brother = parent->children[right];
        }

        /*
         * Brother has no red child. Recolor and repeat at parent.
         */
        if (((brother->children[RBTREE_LEFT] == NULL)
             || rbtree_is_black(brother->children[RBTREE_LEFT]))
            && ((brother->children[RBTREE_RIGHT] == NULL)
                || rbtree_is_black(brother->children[RBTREE_RIGHT]))) {
            rbtree_set_red(brother);
            child = parent;
            parent = rbtree_parent(child);
            continue;
        }

        /*
         * Brother's right child is black. Recolor and rotate right at brother.
         */
        if ((brother->children[right] == NULL)
            || rbtree_is_black(brother->children[right])) {
            rbtree_set_black(brother->children[left]);
            rbtree_set_red(brother);
            rbtree_rotate(tree, brother, right);
            brother = parent->children[right];
        }

        /*
         * Brother's left child is black. Exchange parent and brother colors
         * (we already know brother is black), set brother's right child black,
         * rotate left at parent and leave.
         */
        rbtree_set_color(brother, rbtree_color(parent));
        rbtree_set_black(parent);
        rbtree_set_black(brother->children[right]);
        rbtree_rotate(tree, parent, left);
        break;
    }

    assert((tree->root == NULL) || rbtree_is_black(tree->root));
}

struct rbtree_node * rbtree_nearest(struct rbtree_node *parent, int index,
                                    int direction)
{
    assert(rbtree_check_index(direction));

    if (parent == NULL)
        return NULL;

    assert(rbtree_check_index(index));

    if (index != direction)
        return parent;

    return rbtree_walk(parent, direction);
}

struct rbtree_node * rbtree_firstlast(const struct rbtree *tree, int direction)
{
    struct rbtree_node *prev, *cur;

    assert(rbtree_check_index(direction));

    prev = NULL;

    for (cur = tree->root; cur != NULL; cur = cur->children[direction])
        prev = cur;

    return prev;
}

struct rbtree_node * rbtree_walk(struct rbtree_node *node, int direction)
{
    int left, right;

    assert(rbtree_check_index(direction));

    left = direction;
    right = 1 - left;

    if (node == NULL)
        return NULL;

    if (node->children[left] != NULL) {
        node = node->children[left];

        while (node->children[right] != NULL)
            node = node->children[right];
    } else {
        struct rbtree_node *parent;
        int index;

        for (;;) {
            parent = rbtree_parent(node);

            if (parent == NULL)
                return NULL;

            index = rbtree_index(node, parent);
            node = parent;

            if (index == right)
                break;
        }
    }

    return node;
}

/*
 * Return the left-most deepest child node of the given node.
 */
static struct rbtree_node * rbtree_find_deepest(struct rbtree_node *node)
{
    struct rbtree_node *parent;

    assert(node != NULL);

    for (;;) {
        parent = node;
        node = node->children[RBTREE_LEFT];

        if (node == NULL) {
            node = parent->children[RBTREE_RIGHT];

            if (node == NULL)
                return parent;
        }
    }
}

struct rbtree_node * rbtree_postwalk_deepest(const struct rbtree *tree)
{
    struct rbtree_node *node;

    node = tree->root;

    if (node == NULL)
        return NULL;

    return rbtree_find_deepest(node);
}

struct rbtree_node * rbtree_postwalk_unlink(struct rbtree_node *node)
{
    struct rbtree_node *parent;
    int index;

    if (node == NULL)
        return NULL;

    assert(node->children[RBTREE_LEFT] == NULL);
    assert(node->children[RBTREE_RIGHT] == NULL);

    parent = rbtree_parent(node);

    if (parent == NULL)
        return NULL;

    index = rbtree_index(node, parent);
    parent->children[index] = NULL;
    node = parent->children[RBTREE_RIGHT];

    if (node == NULL)
        return parent;

    return rbtree_find_deepest(node);
}
