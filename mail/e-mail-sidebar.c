/*
 * e-mail-sidebar.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-sidebar.h"

#include <string.h>
#include <camel/camel.h>

#include "mail/em-utils.h"

#define E_MAIL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SIDEBAR, EMailSidebarPrivate))

struct _EMailSidebarPrivate {
	GKeyFile *key_file;  /* not owned */
};

enum {
	PROP_0,
	PROP_KEY_FILE
};

enum {
	KEY_FILE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EMailSidebar,
	e_mail_sidebar,
	EM_TYPE_FOLDER_TREE)

static void
mail_sidebar_restore_state (EMailSidebar *sidebar)
{
	EMFolderTree *folder_tree;
	GKeyFile *key_file;
	gchar *selected;

	key_file = e_mail_sidebar_get_key_file (sidebar);

	/* Make sure we have a key file to restore state from. */
	if (key_file == NULL)
		return;

	folder_tree = EM_FOLDER_TREE (sidebar);

	/* Restore selected folder. */

	selected = g_key_file_get_string (
		key_file, "Folder Tree", "Selected", NULL);
	if (selected != NULL) {
		em_folder_tree_set_selected (folder_tree, selected, FALSE);
		g_free (selected);
	}

	em_folder_tree_restore_state (folder_tree, key_file);
}

static void
mail_sidebar_model_loaded_row_cb (GtkTreeModel *model,
                                  GtkTreePath *path,
                                  GtkTreeIter *iter,
                                  EMailSidebar *sidebar)
{
	GtkTreeView *tree_view;
	GKeyFile *key_file;
	gboolean expanded;
	gboolean is_folder;
	gboolean is_store;
	const gchar *key;
	gchar *group_name;
	gchar *uri;

	tree_view = GTK_TREE_VIEW (sidebar);
	key_file = e_mail_sidebar_get_key_file (sidebar);

	/* Make sure we have a key file to record state changes. */
	if (key_file == NULL)
		return;

	gtk_tree_model_get (
		model, iter,
		COL_STRING_URI, &uri,
		COL_BOOL_IS_STORE, &is_store,
		COL_BOOL_IS_FOLDER, &is_folder, -1);

	g_return_if_fail (is_store || is_folder);

	key = STATE_KEY_EXPANDED;
	if (is_store) {
		group_name = g_strdup_printf ("Store %s", uri);
		expanded = TRUE;
	} else {
		group_name = g_strdup_printf ("Folder %s", uri);
		expanded = FALSE;
	}

	if (g_key_file_has_key (key_file, group_name, key, NULL))
		expanded = g_key_file_get_boolean (
			key_file, group_name, key, NULL);

	if (expanded)
		gtk_tree_view_expand_row (tree_view, path, FALSE);

	g_free (group_name);
	g_free (uri);
}

static void
mail_sidebar_selection_changed_cb (GtkTreeSelection *selection,
                                   EMailSidebar *sidebar)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GKeyFile *key_file;
	gchar *uri = NULL;

	key_file = e_mail_sidebar_get_key_file (sidebar);

	/* Make sure we have a key file to record state changes. */
	if (key_file == NULL)
		return;

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_tree_model_get (model, &iter, COL_STRING_URI, &uri, -1);

	if (uri != NULL)
		g_key_file_set_string (
			key_file, "Folder Tree", "Selected", uri);
	else
		g_key_file_remove_key (
			key_file, "Folder Tree", "Selected", NULL);

	e_mail_sidebar_key_file_changed (sidebar);

	g_free (uri);
}

static void
mail_sidebar_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_KEY_FILE:
			e_mail_sidebar_set_key_file (
				E_MAIL_SIDEBAR (object),
				g_value_get_pointer (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_sidebar_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_KEY_FILE:
			g_value_set_pointer (
				value, e_mail_sidebar_get_key_file (
				E_MAIL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_sidebar_constructed (GObject *object)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;

	/* Chain up to parent's constructed() property. */
	G_OBJECT_CLASS (e_mail_sidebar_parent_class)->constructed (object);

	tree_view = GTK_TREE_VIEW (object);
	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	em_folder_tree_model_set_selection (
		EM_FOLDER_TREE_MODEL (model), selection);

	g_signal_connect (
		model, "loaded-row",
		G_CALLBACK (mail_sidebar_model_loaded_row_cb), object);

	g_signal_connect (
		selection, "changed",
		G_CALLBACK (mail_sidebar_selection_changed_cb), object);
}

static void
mail_sidebar_row_expanded (GtkTreeView *tree_view,
                           GtkTreeIter *unused,
                           GtkTreePath *path)
{
	GtkTreeViewClass *tree_view_class;
	EMailSidebar *sidebar;
	GtkTreeModel *model;
	GKeyFile *key_file;
	const gchar *key;
	gboolean is_folder;
	gboolean is_store;
	gchar *group_name;
	gchar *uri;

	/* Chain up to parent's row_expanded() method.  Do this first
	 * because we stomp on the path argument a few lines down. */
	tree_view_class = GTK_TREE_VIEW_CLASS (e_mail_sidebar_parent_class);
	tree_view_class->row_expanded (tree_view, unused, path);

	sidebar = E_MAIL_SIDEBAR (tree_view);
	key_file = e_mail_sidebar_get_key_file (sidebar);

	/* Make sure we have a key file to record state changes. */
	if (key_file == NULL)
		return;

	path = gtk_tree_path_copy (path);
	model = gtk_tree_view_get_model (tree_view);

	/* Expand the node and all ancestors. */
	while (gtk_tree_path_get_depth (path) > 0) {
		GtkTreeIter iter;

		gtk_tree_model_get_iter (model, &iter, path);

		gtk_tree_model_get (
			model, &iter,
			COL_STRING_URI, &uri,
			COL_BOOL_IS_STORE, &is_store,
			COL_BOOL_IS_FOLDER, &is_folder, -1);

		g_return_if_fail (is_store || is_folder);

		key = STATE_KEY_EXPANDED;
		if (is_store)
			group_name = g_strdup_printf ("Store %s", uri);
		else
			group_name = g_strdup_printf ("Folder %s", uri);

		g_key_file_set_boolean (key_file, group_name, key, TRUE);
		e_mail_sidebar_key_file_changed (sidebar);

		g_free (group_name);
		g_free (uri);

		gtk_tree_path_up (path);
	}

	gtk_tree_path_free (path);
}

static void
mail_sidebar_row_collapsed (GtkTreeView *tree_view,
                            GtkTreeIter *iter,
                            GtkTreePath *path)
{
	EMailSidebar *sidebar;
	GtkTreeModel *model;
	GKeyFile *key_file;
	const gchar *key;
	gboolean is_folder;
	gboolean is_store;
	gchar *group_name;
	gchar *uri;

	sidebar = E_MAIL_SIDEBAR (tree_view);
	key_file = e_mail_sidebar_get_key_file (sidebar);

	/* Make sure we have a key file to record state changes. */
	if (key_file == NULL)
		return;

	model = gtk_tree_view_get_model (tree_view);

	gtk_tree_model_get (
		model, iter,
		COL_STRING_URI, &uri,
		COL_BOOL_IS_STORE, &is_store,
		COL_BOOL_IS_FOLDER, &is_folder, -1);

	g_return_if_fail (is_store || is_folder);

	key = STATE_KEY_EXPANDED;
	if (is_store)
		group_name = g_strdup_printf ("Store %s", uri);
	else
		group_name = g_strdup_printf ("Folder %s", uri);

	g_key_file_set_boolean (key_file, group_name, key, FALSE);
	e_mail_sidebar_key_file_changed (sidebar);

	g_free (group_name);
	g_free (uri);
}

static guint32
mail_sidebar_check_state (EMailSidebar *sidebar)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelStore *store;
	gchar *full_name;
	const gchar *uid;
	gboolean store_is_local;
	gboolean allows_children = TRUE;
	gboolean can_delete = TRUE;
	gboolean is_junk = FALSE;
	gboolean is_outbox = FALSE;
	gboolean is_store;
	gboolean is_trash = FALSE;
	guint32 folder_flags = 0;
	guint32 state = 0;

	tree_view = GTK_TREE_VIEW (sidebar);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return 0;

	gtk_tree_model_get (
		model, &iter,
		COL_POINTER_CAMEL_STORE, &store,
		COL_STRING_FULL_NAME, &full_name,
		COL_BOOL_IS_STORE, &is_store,
		COL_UINT_FLAGS, &folder_flags, -1);

	uid = camel_service_get_uid (CAMEL_SERVICE (store));
	store_is_local = (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0);

	if (!is_store && full_name != NULL) {
		guint32 folder_type;

		/* Is this a virtual junk or trash folder? */
		is_junk = (strcmp (full_name, CAMEL_VJUNK_NAME) == 0);
		is_trash = (strcmp (full_name, CAMEL_VTRASH_NAME) == 0);

		/* Is this a real trash folder?
		 * Used by Exchange and GroupWise accounts. */
		folder_type = (folder_flags & CAMEL_FOLDER_TYPE_MASK);
		is_trash |= (folder_type == CAMEL_FOLDER_TYPE_TRASH);

		allows_children = !(is_junk || is_trash);

		/* Don't allow deletion of special local folders. */
		if (store_is_local) {
			can_delete =
				(strcmp (full_name, "Drafts") != 0) &&
				(strcmp (full_name, "Inbox") != 0) &&
				(strcmp (full_name, "Outbox") != 0) &&
				(strcmp (full_name, "Sent") != 0) &&
				(strcmp (full_name, "Templates") != 0);
			is_outbox =
				(strcmp (full_name, "Outbox") == 0);
		}

		can_delete &= !(folder_flags & CAMEL_FOLDER_SYSTEM);
	}

	if (allows_children)
		state |= E_MAIL_SIDEBAR_FOLDER_ALLOWS_CHILDREN;
	if (can_delete)
		state |= E_MAIL_SIDEBAR_FOLDER_CAN_DELETE;
	if (is_junk)
		state |= E_MAIL_SIDEBAR_FOLDER_IS_JUNK;
	if (is_outbox)
		state |= E_MAIL_SIDEBAR_FOLDER_IS_OUTBOX;
	if (is_store)
		state |= E_MAIL_SIDEBAR_FOLDER_IS_STORE;
	if (is_trash)
		state |= E_MAIL_SIDEBAR_FOLDER_IS_TRASH;
	if (CAMEL_IS_SUBSCRIBABLE (store))
		state |= E_MAIL_SIDEBAR_STORE_IS_SUBSCRIBABLE;

	g_free (full_name);

	return state;
}

static void
e_mail_sidebar_class_init (EMailSidebarClass *class)
{
	GObjectClass *object_class;
	GtkTreeViewClass *tree_view_class;

	g_type_class_add_private (class, sizeof (EMailSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_sidebar_set_property;
	object_class->get_property = mail_sidebar_get_property;
	object_class->constructed = mail_sidebar_constructed;

	tree_view_class = GTK_TREE_VIEW_CLASS (class);
	tree_view_class->row_expanded = mail_sidebar_row_expanded;
	tree_view_class->row_collapsed = mail_sidebar_row_collapsed;

	class->check_state = mail_sidebar_check_state;

	g_object_class_install_property (
		object_class,
		PROP_KEY_FILE,
		g_param_spec_pointer (
			"key-file",
			"Key File",
			NULL,
			G_PARAM_READWRITE));

	signals[KEY_FILE_CHANGED] = g_signal_new (
		"key-file-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailSidebarClass, key_file_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_sidebar_init (EMailSidebar *sidebar)
{
	EMFolderTree *folder_tree;

	sidebar->priv = E_MAIL_SIDEBAR_GET_PRIVATE (sidebar);

	folder_tree = EM_FOLDER_TREE (sidebar);
	em_folder_tree_set_excluded (folder_tree, 0);
	em_folder_tree_enable_drag_and_drop (folder_tree);
}

GtkWidget *
e_mail_sidebar_new (EMailBackend *backend,
                    EAlertSink *alert_sink)
{
	EMFolderTreeModel *model;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);
	g_return_val_if_fail (E_IS_ALERT_SINK (alert_sink), NULL);

	model = em_folder_tree_model_get_default ();

	return g_object_new (
		E_TYPE_MAIL_SIDEBAR,
		"alert-sink", alert_sink,
		"backend", backend,
		"model", model, NULL);
}

GKeyFile *
e_mail_sidebar_get_key_file (EMailSidebar *sidebar)
{
	g_return_val_if_fail (E_IS_MAIL_SIDEBAR (sidebar), NULL);

	return sidebar->priv->key_file;
}

void
e_mail_sidebar_set_key_file (EMailSidebar *sidebar,
                             GKeyFile *key_file)
{
	g_return_if_fail (E_IS_MAIL_SIDEBAR (sidebar));

	/* XXX GKeyFile has no reference count, so all we can do is
	 *     replace the old pointer and hope the key file is not
	 *     freed on us.  Most other GLib data structures have
	 *     grown reference counts so maybe this should too. */
	sidebar->priv->key_file = key_file;

	mail_sidebar_restore_state (sidebar);

	g_object_notify (G_OBJECT (sidebar), "key-file");
}

guint32
e_mail_sidebar_check_state (EMailSidebar *sidebar)
{
	EMailSidebarClass *class;

	g_return_val_if_fail (E_IS_MAIL_SIDEBAR (sidebar), 0);

	class = E_MAIL_SIDEBAR_GET_CLASS (sidebar);
	g_return_val_if_fail (class->check_state != NULL, 0);

	return class->check_state (sidebar);
}

void
e_mail_sidebar_key_file_changed (EMailSidebar *sidebar)
{
	g_return_if_fail (E_IS_MAIL_SIDEBAR (sidebar));

	g_signal_emit (sidebar, signals[KEY_FILE_CHANGED], 0);
}
