/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-tree-memory.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-xml-utils.h"

#define E_TREE_MEMORY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TREE_MEMORY, ETreeMemoryPrivate))

G_DEFINE_TYPE (ETreeMemory, e_tree_memory, E_TYPE_TREE_MODEL)

enum {
	FILL_IN_CHILDREN,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

typedef struct ETreeMemoryPath ETreeMemoryPath;

struct ETreeMemoryPath {
	gpointer         node_data;

	guint            children_computed : 1;

	/* parent/child/sibling pointers */
	ETreeMemoryPath *parent;
	ETreeMemoryPath *next_sibling;
	ETreeMemoryPath *prev_sibling;
	ETreeMemoryPath *first_child;
	ETreeMemoryPath *last_child;

	gint             num_children;
};

struct _ETreeMemoryPrivate {
	ETreeMemoryPath *root;

	/* whether nodes are created expanded
	 * or collapsed by default */
	gboolean         expanded_default;

	gint             frozen;
	GFunc            destroy_func;
	gpointer         destroy_user_data;
};

/* ETreeMemoryPath functions */

static inline void
check_children (ETreeMemory *memory,
                ETreePath node)
{
	ETreeMemoryPath *path = node;
	if (!path->children_computed) {
		g_signal_emit (memory, signals[FILL_IN_CHILDREN], 0, node);
		path->children_computed = TRUE;
	}
}

static gint
e_tree_memory_path_depth (ETreeMemoryPath *path)
{
	gint depth = 0;

	g_return_val_if_fail (path != NULL, -1);

	for (path = path->parent; path; path = path->parent)
		depth++;
	return depth;
}

static void
e_tree_memory_path_insert (ETreeMemoryPath *parent,
                           gint position,
                           ETreeMemoryPath *child)
{
	g_return_if_fail (position <= parent->num_children && position >= -1);

	child->parent = parent;

	if (parent->first_child == NULL)
		parent->first_child = child;

	if (position == -1 || position == parent->num_children) {
		child->prev_sibling = parent->last_child;
		if (parent->last_child)
			parent->last_child->next_sibling = child;
		parent->last_child = child;
	} else {
		ETreeMemoryPath *c;
		for (c = parent->first_child; c; c = c->next_sibling) {
			if (position == 0) {
				child->next_sibling = c;
				child->prev_sibling = c->prev_sibling;

				if (child->next_sibling)
					child->next_sibling->prev_sibling = child;
				if (child->prev_sibling)
					child->prev_sibling->next_sibling = child;

				if (parent->first_child == c)
					parent->first_child = child;
				break;
			}
			position--;
		}
	}

	parent->num_children++;
}

static void
e_tree_path_unlink (ETreeMemoryPath *path)
{
	ETreeMemoryPath *parent = path->parent;

	/* unlink first/last child if applicable */
	if (parent) {
		if (path == parent->first_child)
			parent->first_child = path->next_sibling;
		if (path == parent->last_child)
			parent->last_child = path->prev_sibling;

		parent->num_children--;
	}

	/* unlink prev/next sibling links */
	if (path->next_sibling)
		path->next_sibling->prev_sibling = path->prev_sibling;
	if (path->prev_sibling)
		path->prev_sibling->next_sibling = path->next_sibling;

	path->parent = NULL;
	path->next_sibling = NULL;
	path->prev_sibling = NULL;
}

/**
 * e_tree_memory_freeze:
 * @tree_memory: the ETreeModel to freeze.
 *
 * This function prepares an ETreeModel for a period of much change.
 * All signals regarding changes to the tree are deferred until we
 * thaw the tree.
 *
 **/
void
e_tree_memory_freeze (ETreeMemory *tree_memory)
{
	ETreeMemoryPrivate *priv = tree_memory->priv;

	if (priv->frozen == 0)
		e_tree_model_pre_change (E_TREE_MODEL (tree_memory));

	priv->frozen++;
}

/**
 * e_tree_memory_thaw:
 * @tree_memory: the ETreeMemory to thaw.
 *
 * This function thaws an ETreeMemory.  All the defered signals can add
 * up to a lot, we don't know - so we just emit a model_changed
 * signal.
 *
 **/
void
e_tree_memory_thaw (ETreeMemory *tree_memory)
{
	ETreeMemoryPrivate *priv = tree_memory->priv;

	if (priv->frozen > 0)
		priv->frozen--;
	if (priv->frozen == 0) {
		e_tree_model_node_changed (E_TREE_MODEL (tree_memory), priv->root);
	}
}

/* virtual methods */

static void
tree_memory_dispose (GObject *object)
{
	ETreeMemoryPrivate *priv;

	priv = E_TREE_MEMORY_GET_PRIVATE (object);

	if (priv->root)
		e_tree_memory_node_remove (
			E_TREE_MEMORY (object), priv->root);

	G_OBJECT_CLASS (e_tree_memory_parent_class)->dispose (object);
}

static ETreePath
tree_memory_get_root (ETreeModel *etm)
{
	ETreeMemoryPrivate *priv = E_TREE_MEMORY (etm)->priv;
	return priv->root;
}

static ETreePath
tree_memory_get_parent (ETreeModel *etm,
                 ETreePath node)
{
	ETreeMemoryPath *path = node;
	return path->parent;
}

static ETreePath
tree_memory_get_first_child (ETreeModel *etm,
                      ETreePath node)
{
	ETreeMemoryPath *path = node;

	check_children (E_TREE_MEMORY (etm), node);
	return path->first_child;
}

static ETreePath
tree_memory_get_last_child (ETreeModel *etm,
                     ETreePath node)
{
	ETreeMemoryPath *path = node;

	check_children (E_TREE_MEMORY (etm), node);
	return path->last_child;
}

static ETreePath
tree_memory_get_next (ETreeModel *etm,
               ETreePath node)
{
	ETreeMemoryPath *path = node;
	return path->next_sibling;
}

static ETreePath
tree_memory_get_prev (ETreeModel *etm,
               ETreePath node)
{
	ETreeMemoryPath *path = node;
	return path->prev_sibling;
}

static gboolean
tree_memory_is_root (ETreeModel *etm,
              ETreePath node)
{
	ETreeMemoryPath *path = node;
	return e_tree_memory_path_depth (path) == 0;
}

static gboolean
tree_memory_is_expandable (ETreeModel *etm,
                    ETreePath node)
{
	ETreeMemoryPath *path = node;

	check_children (E_TREE_MEMORY (etm), node);
	return path->first_child != NULL;
}

static guint
tree_memory_get_children (ETreeModel *etm,
                   ETreePath node,
                   ETreePath **nodes)
{
	ETreeMemoryPath *path = node;
	guint n_children;

	check_children (E_TREE_MEMORY (etm), node);

	n_children = path->num_children;

	if (nodes) {
		ETreeMemoryPath *p;
		gint i = 0;

		(*nodes) = g_new (ETreePath, n_children);
		for (p = path->first_child; p; p = p->next_sibling) {
			(*nodes)[i++] = p;
		}
	}

	return n_children;
}

static guint
tree_memory_depth (ETreeModel *etm,
            ETreePath path)
{
	return e_tree_memory_path_depth (path);
}

static gboolean
tree_memory_get_expanded_default (ETreeModel *etm)
{
	ETreeMemory *tree_memory = E_TREE_MEMORY (etm);
	ETreeMemoryPrivate *priv = tree_memory->priv;

	return priv->expanded_default;
}

static void
tree_memory_clear_children_computed (ETreeMemoryPath *path)
{
	for (path = path->first_child; path; path = path->next_sibling) {
		path->children_computed = FALSE;
		tree_memory_clear_children_computed (path);
	}
}

static void
tree_memory_node_request_collapse (ETreeModel *etm,
                            ETreePath node)
{
	ETreeModelClass *parent_class;

	if (node)
		tree_memory_clear_children_computed (node);

	parent_class = E_TREE_MODEL_CLASS (e_tree_memory_parent_class);

	if (parent_class->node_request_collapse != NULL)
		parent_class->node_request_collapse (etm, node);
}

static void
e_tree_memory_class_init (ETreeMemoryClass *class)
{
	GObjectClass *object_class;
	ETreeModelClass *tree_model_class;

	g_type_class_add_private (class, sizeof (ETreeMemoryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = tree_memory_dispose;

	tree_model_class = E_TREE_MODEL_CLASS (class);
	tree_model_class->get_root = tree_memory_get_root;
	tree_model_class->get_prev = tree_memory_get_prev;
	tree_model_class->get_next = tree_memory_get_next;
	tree_model_class->get_first_child = tree_memory_get_first_child;
	tree_model_class->get_last_child = tree_memory_get_last_child;
	tree_model_class->get_parent = tree_memory_get_parent;

	tree_model_class->is_root = tree_memory_is_root;
	tree_model_class->is_expandable = tree_memory_is_expandable;
	tree_model_class->get_children = tree_memory_get_children;
	tree_model_class->depth = tree_memory_depth;
	tree_model_class->get_expanded_default = tree_memory_get_expanded_default;

	tree_model_class->node_request_collapse = tree_memory_node_request_collapse;

	class->fill_in_children = NULL;

	signals[FILL_IN_CHILDREN] = g_signal_new (
		"fill_in_children",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeMemoryClass, fill_in_children),
		(GSignalAccumulator) NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);
}

static void
e_tree_memory_init (ETreeMemory *tree_memory)
{
	tree_memory->priv = E_TREE_MEMORY_GET_PRIVATE (tree_memory);
}

/**
 * e_tree_memory_construct:
 * @tree_memory:
 *
 *
 **/
void
e_tree_memory_construct (ETreeMemory *tree_memory)
{
}

/**
 * e_tree_memory_new
 *
 * XXX docs here.
 *
 * return values: a newly constructed ETreeMemory.
 */
ETreeMemory *
e_tree_memory_new (void)
{
	return g_object_new (E_TYPE_TREE_MEMORY, NULL);
}

/**
 * e_tree_memory_set_expanded_default
 *
 * Sets the state of nodes to be append to a thread.
 * They will either be expanded or collapsed, according to
 * the value of @expanded.
 */
void
e_tree_memory_set_expanded_default (ETreeMemory *tree_memory,
                                    gboolean expanded)
{
	g_return_if_fail (tree_memory != NULL);

	tree_memory->priv->expanded_default = expanded;
}

/**
 * e_tree_memory_node_get_data:
 * @tree_memory:
 * @node:
 *
 *
 *
 * Return value:
 **/
gpointer
e_tree_memory_node_get_data (ETreeMemory *tree_memory,
                             ETreePath node)
{
	ETreeMemoryPath *path = node;

	g_return_val_if_fail (path, NULL);

	return path->node_data;
}

/**
 * e_tree_memory_node_set_data:
 * @tree_memory:
 * @node:
 * @node_data:
 *
 *
 **/
void
e_tree_memory_node_set_data (ETreeMemory *tree_memory,
                             ETreePath node,
                             gpointer node_data)
{
	ETreeMemoryPath *path = node;

	g_return_if_fail (path);

	path->node_data = node_data;
}

/**
 * e_tree_memory_node_insert:
 * @tree_memory:
 * @parent_node:
 * @position:
 * @node_data:
 *
 *
 *
 * Return value:
 **/
ETreePath
e_tree_memory_node_insert (ETreeMemory *tree_memory,
                           ETreePath parent_node,
                           gint position,
                           gpointer node_data)
{
	ETreeMemoryPrivate *priv;
	ETreeMemoryPath *new_path;
	ETreeMemoryPath *parent_path = parent_node;

	g_return_val_if_fail (tree_memory != NULL, NULL);

	priv = tree_memory->priv;

	g_return_val_if_fail (parent_path != NULL || priv->root == NULL, NULL);

	priv = tree_memory->priv;

	if (!tree_memory->priv->frozen)
		e_tree_model_pre_change (E_TREE_MODEL (tree_memory));

	new_path = g_slice_new0 (ETreeMemoryPath);

	new_path->node_data = node_data;
	new_path->children_computed = FALSE;

	if (parent_path != NULL) {
		e_tree_memory_path_insert (parent_path, position, new_path);
		if (!tree_memory->priv->frozen)
			e_tree_model_node_inserted (
				E_TREE_MODEL (tree_memory),
				parent_path, new_path);
	} else {
		priv->root = new_path;
		if (!tree_memory->priv->frozen)
			e_tree_model_node_changed (
				E_TREE_MODEL (tree_memory), new_path);
	}

	return new_path;
}

ETreePath
e_tree_memory_node_insert_id (ETreeMemory *tree_memory,
                              ETreePath parent,
                              gint position,
                              gpointer node_data,
                              gchar *id)
{
	return e_tree_memory_node_insert (tree_memory, parent, position, node_data);
}

/**
 * e_tree_memory_node_insert_before:
 * @tree_memory:
 * @parent:
 * @sibling:
 * @node_data:
 *
 *
 *
 * Return value:
 **/
ETreePath
e_tree_memory_node_insert_before (ETreeMemory *tree_memory,
                                  ETreePath parent,
                                  ETreePath sibling,
                                  gpointer node_data)
{
	ETreeMemoryPath *child;
	ETreeMemoryPath *parent_path = parent;
	ETreeMemoryPath *sibling_path = sibling;
	gint position = 0;

	g_return_val_if_fail (tree_memory != NULL, NULL);

	if (sibling != NULL) {
		for (child = parent_path->first_child; child; child = child->next_sibling) {
			if (child == sibling_path)
				break;
			position++;
		}
	} else
		position = parent_path->num_children;
	return e_tree_memory_node_insert (tree_memory, parent, position, node_data);
}

/* just blows away child data, doesn't take into account unlinking/etc */
static void
child_free (ETreeMemory *tree_memory,
            ETreeMemoryPath *node)
{
	ETreeMemoryPath *child, *next;

	child = node->first_child;
	while (child) {
		next = child->next_sibling;
		child_free (tree_memory, child);
		child = next;
	}

	if (tree_memory->priv->destroy_func) {
		tree_memory->priv->destroy_func (node->node_data, tree_memory->priv->destroy_user_data);
	}

	g_slice_free (ETreeMemoryPath, node);
}

/**
 * e_tree_memory_node_remove:
 * @tree_memory:
 * @path:
 *
 *
 *
 * Return value:
 **/
gpointer
e_tree_memory_node_remove (ETreeMemory *tree_memory,
                           ETreePath node)
{
	ETreeMemoryPath *path = node;
	ETreeMemoryPath *parent = path->parent;
	ETreeMemoryPath *sibling;
	gpointer ret = path->node_data;
	gint old_position = 0;

	g_return_val_if_fail (tree_memory != NULL, NULL);

	if (!tree_memory->priv->frozen) {
		e_tree_model_pre_change (E_TREE_MODEL (tree_memory));
		for (old_position = 0, sibling = path;
		     sibling;
		     old_position++, sibling = sibling->prev_sibling)
			/* Empty intentionally*/;
		old_position--;
	}

	/* unlink this node - we only have to unlink the root node being removed,
	 * since the others are only references from this node */
	e_tree_path_unlink (path);

	/*printf("removing %d nodes from position %d\n", visible, base);*/
	if (!tree_memory->priv->frozen)
		e_tree_model_node_removed (E_TREE_MODEL (tree_memory), parent, path, old_position);

	child_free (tree_memory, path);

	if (path == tree_memory->priv->root)
		tree_memory->priv->root = NULL;

	if (!tree_memory->priv->frozen)
		e_tree_model_node_deleted (E_TREE_MODEL (tree_memory), path);

	return ret;
}

typedef struct {
	ETreeMemory *memory;
	gpointer closure;
	ETreeMemorySortCallback callback;
} MemoryAndClosure;

static gint
sort_callback (gconstpointer data1,
               gconstpointer data2,
               gpointer user_data)
{
	ETreePath path1 = *(ETreePath *) data1;
	ETreePath path2 = *(ETreePath *) data2;
	MemoryAndClosure *mac = user_data;
	return (*mac->callback) (mac->memory, path1, path2, mac->closure);
}

void
e_tree_memory_sort_node (ETreeMemory *tree_memory,
                         ETreePath node,
                         ETreeMemorySortCallback callback,
                         gpointer user_data)
{
	ETreeMemoryPath **children;
	ETreeMemoryPath *child;
	gint count;
	gint i;
	ETreeMemoryPath *path = node;
	MemoryAndClosure mac;
	ETreeMemoryPath *last;

	e_tree_model_pre_change (E_TREE_MODEL (tree_memory));

	i = 0;
	for (child = path->first_child; child; child = child->next_sibling)
		i++;

	children = g_new (ETreeMemoryPath *, i);

	count = i;

	for (child = path->first_child, i = 0;
	     child;
	     child = child->next_sibling, i++) {
		children[i] = child;
	}

	mac.memory = tree_memory;
	mac.closure = user_data;
	mac.callback = callback;

	g_qsort_with_data (
		children, count, sizeof (ETreeMemoryPath *),
		sort_callback, &mac);

	path->first_child = NULL;
	last = NULL;
	for (i = 0;
	     i < count;
	     i++) {
		children[i]->prev_sibling = last;
		if (last)
			last->next_sibling = children[i];
		else
			path->first_child = children[i];
		last = children[i];
	}
	if (last)
		last->next_sibling = NULL;

	path->last_child = last;

	g_free (children);

	e_tree_model_node_changed (E_TREE_MODEL (tree_memory), node);
}

void
e_tree_memory_set_node_destroy_func (ETreeMemory *tree_memory,
                                     GFunc destroy_func,
                                     gpointer user_data)
{
	tree_memory->priv->destroy_func = destroy_func;
	tree_memory->priv->destroy_user_data = user_data;
}
