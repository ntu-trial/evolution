/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-message-cache.c: Class for an IMAP message cache */

/* 
 * Author: 
 *   Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "camel-imap-message-cache.h"
#include "camel-exception.h"
#include "camel-stream-fs.h"

static void finalize (CamelImapMessageCache *cache);
static void stream_finalize (CamelObject *stream, gpointer event_data, gpointer user_data);


CamelType
camel_imap_message_cache_get_type (void)
{
	static CamelType camel_imap_message_cache_type = CAMEL_INVALID_TYPE;
	
	if (camel_imap_message_cache_type == CAMEL_INVALID_TYPE) {
		camel_imap_message_cache_type = camel_type_register (
			CAMEL_OBJECT_TYPE, "CamelImapMessageCache",
			sizeof (CamelImapMessageCache),
			sizeof (CamelImapMessageCacheClass),
			NULL,
			NULL,
			NULL,
			(CamelObjectFinalizeFunc) finalize);
	}

	return camel_imap_message_cache_type;
}

static void
free_part (gpointer key, gpointer value, gpointer data)
{
	if (value) {
		if (strchr (key, '.')) {
			camel_object_unhook_event (value, "finalize",
						   stream_finalize, data);
			camel_object_unref (value);
		} else
			g_ptr_array_free (value, TRUE);
	}
	g_free (key);
}

static void
finalize (CamelImapMessageCache *cache)
{
	if (cache->path)
		g_free (cache->path);
	if (cache->parts) {
		g_hash_table_foreach (cache->parts, free_part, cache);
		g_hash_table_destroy (cache->parts);
	}
	if (cache->cached)
		g_hash_table_destroy (cache->cached);
}

static void
cache_put (CamelImapMessageCache *cache, const char *uid, const char *key,
	   CamelStream *stream)
{
	char *hash_key;
	GPtrArray *subparts;
	gpointer old_key, old_value;

	hash_key = g_strdup (key);
	subparts = g_hash_table_lookup (cache->parts, uid);
	if (!subparts) {
		subparts = g_ptr_array_new ();
		g_hash_table_insert (cache->parts, g_strdup (uid), subparts);
	} else if (g_hash_table_lookup_extended (cache->parts, hash_key,
						 &old_key, &old_value))
		g_ptr_array_remove (subparts, old_key);

	g_ptr_array_add (subparts, hash_key);
	g_hash_table_insert (cache->parts, hash_key, stream);
	g_hash_table_insert (cache->cached, stream, hash_key);

	if (stream) {
		camel_object_hook_event (CAMEL_OBJECT (stream), "finalize",
					 stream_finalize, cache);
	}
}

CamelImapMessageCache *
camel_imap_message_cache_new (const char *path, CamelFolderSummary *summary,
			      CamelException *ex)
{
	CamelImapMessageCache *cache;
	DIR *dir;
	struct dirent *d;
	char *uid, *p;
	GPtrArray *deletes;
	CamelMessageInfo *info;

	dir = opendir (path);
	if (!dir) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not open cache directory: %s"),
				      g_strerror (errno));
		return NULL;
	}

	cache = (CamelImapMessageCache *)camel_object_new (CAMEL_IMAP_MESSAGE_CACHE_TYPE);
	cache->path = g_strdup (path);

	cache->parts = g_hash_table_new (g_str_hash, g_str_equal);
	cache->cached = g_hash_table_new (NULL, NULL);
	deletes = g_ptr_array_new ();
	while ((d = readdir (dir))) {
		if (!isdigit (d->d_name[0]))
			continue;

		p = strchr (d->d_name, '.');
		if (p)
			uid = g_strndup (d->d_name, p - d->d_name);
		else
			uid = g_strdup (d->d_name);

		info = camel_folder_summary_uid (summary, uid);
		if (info) {
			camel_folder_summary_info_free (summary, info);
			cache_put (cache, uid, d->d_name, NULL);
		} else
			g_ptr_array_add (deletes, g_strdup_printf ("%s/%s", cache->path, d->d_name));
		g_free (uid);
	}
	closedir (dir);

	while (deletes->len) {
		unlink (deletes->pdata[0]);
		g_free (deletes->pdata[0]);
		g_ptr_array_remove_index_fast (deletes, 0);
	}
	g_ptr_array_free (deletes, TRUE);

	if (camel_exception_is_set (ex)) {
		camel_object_unref (CAMEL_OBJECT (cache));
		return NULL;
	}

	return cache;
}


static void
stream_finalize (CamelObject *stream, gpointer event_data, gpointer user_data)
{
	CamelImapMessageCache *cache = user_data;
	char *key;

	key = g_hash_table_lookup (cache->cached, stream);
	if (!key)
		return;
	g_hash_table_remove (cache->cached, stream);
	g_hash_table_insert (cache->parts, key, NULL);
}

CamelStream *
camel_imap_message_cache_insert (CamelImapMessageCache *cache, const char *uid,
				 const char *part_spec, const char *data,
				 int len)
{
	char *path, *key;
	int fd, status;
	CamelStream *stream;

	path = g_strdup_printf ("%s/%s.%s", cache->path, uid, part_spec);
	key = strrchr (path, '/') + 1;
	stream = g_hash_table_lookup (cache->parts, key);
	if (stream)
		camel_object_unref (CAMEL_OBJECT (stream));

	fd = open (path, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd == -1) {
		g_free (path);
		return NULL;
	}

	stream = camel_stream_fs_new_with_fd (fd);
	status = camel_stream_write (stream, data, len);
	camel_stream_reset (stream);

	if (status == -1) {
		unlink (path);
		g_free (path);
		camel_object_unref (CAMEL_OBJECT (stream));
		return NULL;
	}

	cache_put (cache, uid, key, stream);
	g_free (path);

	return stream;
}

CamelStream *
camel_imap_message_cache_get (CamelImapMessageCache *cache, const char *uid,
			      const char *part_spec)
{
	CamelStream *stream;
	char *path, *key;

	path = g_strdup_printf ("%s/%s.%s", cache->path, uid, part_spec);
	key = strrchr (path, '/');
	stream = g_hash_table_lookup (cache->parts, key);
	if (stream) {
		camel_object_ref (CAMEL_OBJECT (stream));
		return stream;
	}

	stream = camel_stream_fs_new_with_name (path, O_RDONLY, 0);
	if (stream)
		cache_put (cache, uid, key, stream);
	g_free (path);

	return stream;
}

void
camel_imap_message_cache_remove (CamelImapMessageCache *cache, const char *uid)
{
	GPtrArray *subparts;
	char *key, *path;
	CamelObject *stream;
	int i;

	subparts = g_hash_table_lookup (cache->parts, uid);
	if (!subparts)
		return;
	for (i = 0; i < subparts->len; i++) {
		key = subparts->pdata[i];
		path = g_strdup_printf ("%s/%s", cache->path, key);
		unlink (path);
		g_free (path);
		stream = g_hash_table_lookup (cache->parts, key);
		if (stream) {
			camel_object_unhook_event (stream, "finalize",
						   stream_finalize, cache);
			camel_object_unref (stream);
			g_hash_table_remove (cache->cached, stream);
		}
		g_hash_table_remove (cache->parts, key);
		g_free (key);
	}
	g_hash_table_remove (cache->parts, uid);
	g_ptr_array_free (subparts, TRUE);
}

static gboolean
clear_part (gpointer key, gpointer value, gpointer data)
{
	if (!strchr (key, '.'))
		camel_imap_message_cache_remove (data, key);
	return TRUE;
}

void
camel_imap_message_cache_clear (CamelImapMessageCache *cache)
{
	g_hash_table_foreach_remove (cache->parts, clear_part, cache);
}
