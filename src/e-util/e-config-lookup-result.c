/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: e-config-lookup-result
 * @include: e-util/e-util.h
 * @short_description: Configuration lookup result interface
 *
 * #EConfigLookupResult is an interface which actual results need to implement.
 * Such result holds information about one kind and knows how to setup
 * an #ESource with the looked up values.
 *
 * Simple changes can be saved using #EConfigLookupResultSimple object.
 **/

#include "evolution-config.h"

#include <libedataserver/libedataserver.h>

#include "e-util-enums.h"

#include "e-config-lookup-result.h"

G_DEFINE_INTERFACE (EConfigLookupResult, e_config_lookup_result, G_TYPE_OBJECT)

static void
e_config_lookup_result_default_init (EConfigLookupResultInterface *iface)
{
	iface->get_kind = NULL;
	iface->get_priority = NULL;
	iface->get_protocol = NULL;
	iface->get_display_name = NULL;
	iface->get_description = NULL;
	iface->configure_source = NULL;
}

/**
 * e_config_lookup_result_get_kind:
 * @lookup_result: an #EConfigLookupResult
 *
 * Returns: the result kind, one of #EConfigLookupResultKind, this lookup result corresponds to
 *
 * Since: 3.26
 **/
EConfigLookupResultKind
e_config_lookup_result_get_kind (EConfigLookupResult *lookup_result)
{
	EConfigLookupResultInterface *iface;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT (lookup_result), E_CONFIG_LOOKUP_RESULT_UNKNOWN);

	iface = E_CONFIG_LOOKUP_RESULT_GET_INTERFACE (lookup_result);
	g_return_val_if_fail (iface != NULL, E_CONFIG_LOOKUP_RESULT_UNKNOWN);
	g_return_val_if_fail (iface->get_kind != NULL, E_CONFIG_LOOKUP_RESULT_UNKNOWN);

	return iface->get_kind (lookup_result);
}

/**
 * e_config_lookup_result_get_priority:
 * @lookup_result: an #EConfigLookupResult
 *
 * Returns: the result priority; lower value means higher priority
 *
 * Since: 3.26
 **/
gint
e_config_lookup_result_get_priority (EConfigLookupResult *lookup_result)
{
	EConfigLookupResultInterface *iface;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT (lookup_result), ~0);

	iface = E_CONFIG_LOOKUP_RESULT_GET_INTERFACE (lookup_result);
	g_return_val_if_fail (iface != NULL, ~0);
	g_return_val_if_fail (iface->get_priority != NULL, ~0);

	return iface->get_priority (lookup_result);
}

/**
 * e_config_lookup_result_get_protocol:
 * @lookup_result: an #EConfigLookupResult
 *
 * Returns: (nullable): if applicable, returns the protocol of this @lookup_result,
 *    or %NULL if not set, or not known, or not applicable
 *
 * Since: 3.26
 **/
const gchar *
e_config_lookup_result_get_protocol (EConfigLookupResult *lookup_result)
{
	EConfigLookupResultInterface *iface;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT (lookup_result), NULL);

	iface = E_CONFIG_LOOKUP_RESULT_GET_INTERFACE (lookup_result);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->get_protocol != NULL, NULL);

	return iface->get_protocol (lookup_result);
}

/**
 * e_config_lookup_result_get_display_name:
 * @lookup_result: an #EConfigLookupResult
 *
 * Returns: human readable display name of this @lookup_result
 *
 * Since: 3.26
 **/
const gchar *
e_config_lookup_result_get_display_name (EConfigLookupResult *lookup_result)
{
	EConfigLookupResultInterface *iface;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT (lookup_result), NULL);

	iface = E_CONFIG_LOOKUP_RESULT_GET_INTERFACE (lookup_result);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->get_display_name != NULL, NULL);

	return iface->get_display_name (lookup_result);
}

/**
 * e_config_lookup_result_get_description:
 * @lookup_result: an #EConfigLookupResult
 *
 * Returns: human readable description of this @lookup_result
 *
 * Since: 3.26
 **/
const gchar *
e_config_lookup_result_get_description (EConfigLookupResult *lookup_result)
{
	EConfigLookupResultInterface *iface;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT (lookup_result), NULL);

	iface = E_CONFIG_LOOKUP_RESULT_GET_INTERFACE (lookup_result);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->get_description != NULL, NULL);

	return iface->get_description (lookup_result);
}

/**
 * e_config_lookup_result_configure_source:
 * @lookup_result: an #EConfigLookupResult
 * @source: an #ESource to configure
 *
 * Configures the @source with the looked up configuration.
 *
 * Returns: %TRUE when made any changes to the @source, %FALSE otherwise
 *
 * Since: 3.26
 **/
gboolean
e_config_lookup_result_configure_source (EConfigLookupResult *lookup_result,
					 ESource *source)
{
	EConfigLookupResultInterface *iface;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT (lookup_result), FALSE);

	iface = E_CONFIG_LOOKUP_RESULT_GET_INTERFACE (lookup_result);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->configure_source != NULL, FALSE);

	return iface->configure_source (lookup_result, source);
}

/**
 * e_config_lookup_result_compare:
 * @lookup_result_a: the first #EConfigLookupResult
 * @lookup_result_b: the second #EConfigLookupResult
 *
 * Compares two #EConfigLookupResult objects, and returns value less than 0,
 * when @lookup_result_a is before @lookup_result_b, 0 when they are the same
 * and value greater than 0, when @lookup_result_a is after @lookup_result_b.
 *
 * The comparison is done on kind, priority and display name values, in this order.
 *
 * Returns: strcmp()-like value, what the position between @lookup_result_a and
 *    @lookup_result_b is.
 *
 * Since: 3.26
 **/
gint
e_config_lookup_result_compare (gconstpointer lookup_result_a,
				gconstpointer lookup_result_b)
{
	EConfigLookupResult *lra, *lrb;
	gint res;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT (lookup_result_a), 0);
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_RESULT (lookup_result_b), 0);

	lra = E_CONFIG_LOOKUP_RESULT (lookup_result_a);
	lrb = E_CONFIG_LOOKUP_RESULT (lookup_result_b);

	res = e_config_lookup_result_get_kind (lra) - e_config_lookup_result_get_kind (lrb);

	if (!res)
		res = e_config_lookup_result_get_priority (lra) - e_config_lookup_result_get_priority (lrb);

	if (!res) {
		const gchar *display_name_a, *display_name_b;

		display_name_a = e_config_lookup_result_get_display_name (lra);
		display_name_b = e_config_lookup_result_get_display_name (lrb);

		if (!display_name_a || !display_name_b)
			res = g_strcmp0 (display_name_a, display_name_b);
		else
			res = g_utf8_collate (display_name_a, display_name_b);
	}

	return res;
}
