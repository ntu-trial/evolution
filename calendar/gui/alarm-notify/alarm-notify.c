/* Evolution calendar - Alarm notification service object
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <bonobo/bonobo-main.h>
#include <libecal/e-cal.h>
#include "alarm-notify.h"
#include "alarm-queue.h"
#include "common/authentication.h"
#include "e-util/e-url.h"



/* Private part of the AlarmNotify structure */
struct _AlarmNotifyPrivate {
	/* Mapping from EUri's to LoadedClient structures */
	GHashTable *uri_client_hash;
};



static void alarm_notify_class_init (AlarmNotifyClass *klass);
static void alarm_notify_init (AlarmNotify *an, AlarmNotifyClass *klass);
static void alarm_notify_finalize (GObject *object);


static BonoboObjectClass *parent_class;



BONOBO_TYPE_FUNC_FULL(AlarmNotify, GNOME_Evolution_Calendar_AlarmNotify, BONOBO_TYPE_OBJECT, alarm_notify)

/* Class initialization function for the alarm notify service */
static void
alarm_notify_class_init (AlarmNotifyClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = alarm_notify_finalize;
}

/* Object initialization function for the alarm notify system */
static void
alarm_notify_init (AlarmNotify *an, AlarmNotifyClass *klass)
{
	AlarmNotifyPrivate *priv;

	priv = g_new0 (AlarmNotifyPrivate, 1);
	an->priv = priv;

	priv->uri_client_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

static void
free_client_hash (gpointer key, gpointer value, gpointer user_data)
{
	char *uri = key;
	ECal *client = value;

	alarm_queue_remove_client (client);

	g_free (uri);
	g_object_unref (client);
}

/* Finalize handler for the alarm notify system */
static void
alarm_notify_finalize (GObject *object)
{
	AlarmNotify *an;
	AlarmNotifyPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (object));

	an = ALARM_NOTIFY (object);
	priv = an->priv;

	g_hash_table_foreach (priv->uri_client_hash, (GHFunc) free_client_hash, NULL);
	g_hash_table_destroy (priv->uri_client_hash);

	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/**
 * alarm_notify_new:
 * 
 * Creates a new #AlarmNotify object.
 * 
 * Return value: A newly-created #AlarmNotify, or NULL if its corresponding
 * CORBA object could not be created.
 **/
AlarmNotify *
alarm_notify_new (void)
{
	AlarmNotify *an;

	an = g_object_new (TYPE_ALARM_NOTIFY,
			   "poa", bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL),
			   NULL);
	return an;
}

static void
cal_opened_cb (ECal *client, ECalendarStatus status, gpointer user_data)
{
	AlarmNotifyPrivate *priv;
	AlarmNotify *an = ALARM_NOTIFY (user_data);

	priv = an->priv;

	if (status == E_CALENDAR_STATUS_OK)
		alarm_queue_add_client (client);
	else {
		gpointer orig_key, orig_value;

		if (g_hash_table_lookup_extended (priv->uri_client_hash, e_cal_get_uri (client), &orig_key, &orig_value)) {
			g_hash_table_remove (priv->uri_client_hash, orig_key);
			g_free (orig_key);
		}

		g_object_unref (client);
	}
}

/**
 * alarm_notify_add_calendar:
 * @an: An alarm notification service.
 * @uri: URI of the calendar to load.
 * @load_afterwards: Whether this calendar should be loaded in the future
 * when the alarm daemon starts up.
 * 
 * Tells the alarm notification service to load a calendar and start monitoring
 * its alarms.  It can optionally be made to save the URI of this calendar so
 * that it can be loaded in the future when the alarm daemon starts up.
 **/
void
alarm_notify_add_calendar (AlarmNotify *an, const char *str_uri, gboolean load_afterwards)
{
	AlarmNotifyPrivate *priv;
	ECal *client;

	g_return_if_fail (an != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (an));
	g_return_if_fail (str_uri != NULL);

	priv = an->priv;

	/* See if we already know about this uri */
	if (g_hash_table_lookup (priv->uri_client_hash, str_uri))
		return;

	client = auth_new_cal_from_uri (str_uri, E_CAL_SOURCE_TYPE_EVENT);

	if (client) {
		g_hash_table_insert (priv->uri_client_hash, g_strdup (str_uri), client);
		g_signal_connect (G_OBJECT (client), "cal_opened", G_CALLBACK (cal_opened_cb), an);
		e_cal_open_async (client, FALSE);
	}
}

void
alarm_notify_remove_calendar (AlarmNotify *an, const char *str_uri)
{
	AlarmNotifyPrivate *priv;
	gpointer orig_key, orig_value;

	priv = an->priv;

	if (g_hash_table_lookup_extended (priv->uri_client_hash, str_uri, &orig_key, &orig_value)) {
		alarm_queue_remove_client (E_CAL (orig_value));

		g_hash_table_remove (priv->uri_client_hash, str_uri);
		g_free (orig_key);
		g_object_unref (orig_value);
	}
}
