/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
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
 */

#include "callbacks.h"

#include "cli_infos.h"
#include "cli_cache.h"
#include "column_display.h"


/* Dumps a propdict on stdout */
static void
propdict_dump (const void *key, xmmsc_result_value_type_t type,
               const void *value, const char *source, void *udata)
{
	switch (type) {
	case XMMSC_RESULT_VALUE_TYPE_UINT32:
		g_printf (_("[%s] %s = %u\n"), source, key, (uint32_t*) value);
		break;
	case XMMSC_RESULT_VALUE_TYPE_INT32:
		g_printf (_("[%s] %s = %d\n"), source, key, (int32_t*) value);
		break;
	case XMMSC_RESULT_VALUE_TYPE_STRING:
		g_printf (_("[%s] %s = %s\n"), source, key, (gchar*) value);
		break;
	case XMMSC_RESULT_VALUE_TYPE_DICT:
		g_printf (_("[%s] %s = <dict>\n"), source, key);
		break;
	case XMMSC_RESULT_VALUE_TYPE_PROPDICT:
		g_printf (_("[%s] %s = <propdict>\n"), source, key);
		break;
	case XMMSC_RESULT_VALUE_TYPE_COLL:
		g_printf (_("[%s] %s = <coll>\n"), source, key);
		break;
	case XMMSC_RESULT_VALUE_TYPE_BIN:
		g_printf (_("[%s] %s = <bin>\n"), source, key);
		break;
	case XMMSC_RESULT_VALUE_TYPE_NONE:
		g_printf (_("[%s] %s = <unknown>\n"), source, key);
		break;
	}
}


/* Dummy callback that resets the action status as finished. */
void
cb_done (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	cli_infos_loop_resume (infos);

	if (xmmsc_result_iserror (res)) {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

void
cb_coldisp_finalize (xmmsc_result_t *res, void *udata)
{
	column_display_t *coldisp = (column_display_t *) udata;
	column_display_print_footer (coldisp);
	column_display_free (coldisp);
}

void
cb_tickle (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	xmmsc_result_t *res2;

	if (!xmmsc_result_iserror (res)) {
		res2 = xmmsc_playback_tickle (infos->conn);
		xmmsc_result_notifier_set (res2, cb_done, infos);
		xmmsc_result_unref (res2);
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

void
cb_entry_print_status (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	gchar *artist;
	gchar *title;

	/* FIXME: ad-hoc display, use richer parser */
	if (!xmmsc_result_iserror (res)) {
		if (xmmsc_result_get_dict_entry_string (res, "artist", &artist)
		    && xmmsc_result_get_dict_entry_string (res, "title", &title)) {
			g_printf ("Playing: %s - %s\n", artist, title);
		} else {
			g_printf ("Error getting metadata!\n", artist, title);
		}
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

void
cb_id_print_info (xmmsc_result_t *res, void *udata)
{
	guint id = GPOINTER_TO_UINT(udata);

	if (!xmmsc_result_iserror (res)) {
		g_printf (_("<mid=%u>\n"), id);
		xmmsc_result_propdict_foreach (res, propdict_dump, NULL);
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

void
cb_list_print_info (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	xmmsc_result_t *infores = NULL;
	guint id;

	if (!xmmsc_result_iserror (res)) {
		while (xmmsc_result_list_valid (res)) {
			if (infores) {
				xmmsc_result_unref (infores); /* unref previous infores */
			}
			if (xmmsc_result_get_uint (res, &id)) {
				infores = xmmsc_medialib_get_info (infos->conn, id);
				xmmsc_result_notifier_set (infores, cb_id_print_info,
				                           GUINT_TO_POINTER(id));
			}
			xmmsc_result_list_next (res);
		}

	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	if (!infores) {
		/* Done after the last callback */
		xmmsc_result_notifier_set (infores, cb_done, infos);
		xmmsc_result_unref (infores);
	} else {
		/* No resume-callback pending, we're done */
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
cb_id_print_row (xmmsc_result_t *res, void *udata)
{
	column_display_t *coldisp = (column_display_t *) udata;
	column_display_print (coldisp, res);
	xmmsc_result_unref (res);
}

void
cb_list_print_row (xmmsc_result_t *res, void *udata)
{
	/* FIXME: w00t at code copy-paste, please modularize */
	column_display_t *coldisp = (column_display_t *) udata;
	cli_infos_t *infos = column_display_infos_get (coldisp);
	xmmsc_result_t *infores = NULL;
	guint id;
	gint i = 0;

	if (!xmmsc_result_iserror (res)) {
		column_display_prepare (coldisp);
		column_display_print_header (coldisp);
		while (xmmsc_result_list_valid (res)) {
			if (infores) {
				xmmsc_result_unref (infores); /* unref previous infores */
			}
			if (xmmsc_result_get_uint (res, &id)) {
				infores = xmmsc_medialib_get_info (infos->conn, id);
				xmmsc_result_notifier_set (infores, cb_id_print_row, coldisp);
			}
			xmmsc_result_list_next (res);
			i++;
		}

	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	if (infores) {
		/* Done after the last callback */
		xmmsc_result_notifier_set (infores, cb_coldisp_finalize, coldisp);
		xmmsc_result_notifier_set (infores, cb_done, infos);
		xmmsc_result_unref (infores);
	} else {
		/* No resume-callback pending, we're done */
		column_display_print_footer (coldisp);
		column_display_free (coldisp);
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

/* Abstract jump, use inc to choose the direction. */
static void
cb_list_jump_rel (xmmsc_result_t *res, void *udata, gint inc)
{
	guint i, j;
	guint id;
	cli_infos_t *infos = (cli_infos_t *) udata;
	xmmsc_result_t *jumpres = NULL;

	gint currpos;
	gint plsize;
	GArray *playlist;

	currpos = infos->cache->currpos;
	plsize = infos->cache->active_playlist_size;
	playlist = infos->cache->active_playlist;

	if (!xmmsc_result_iserror (res) && xmmsc_result_list_valid (res)) {

		inc += plsize; /* magic trick so we can loop in either direction */

		/* Loop on the playlist */
		for (i = (currpos + inc) % plsize; i != currpos; i = (i + inc) % plsize) {

			/* Loop on the matched media */
			for (xmmsc_result_list_first (res);
				 xmmsc_result_list_valid (res);
				 xmmsc_result_list_next (res)) {

				/* If both match, jump! */
				if (xmmsc_result_get_uint (res, &id)
				    && g_array_index(playlist, guint, i) == id) {
					jumpres = xmmsc_playlist_set_next (infos->conn, i);
					xmmsc_result_notifier_set (jumpres, cb_tickle, infos);
					xmmsc_result_unref (jumpres);
					goto finish;
				}
			}
		}
	}

	finish:

	/* No matching media found, don't jump */
	if (!jumpres) {
		g_printf (_("No media matching the pattern in the playlist!\n"));
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
cb_list_jump_back (xmmsc_result_t *res, void *udata)
{
	cb_list_jump_rel (res, udata, -1);
}

void
cb_list_jump (xmmsc_result_t *res, void *udata)
{
	cb_list_jump_rel (res, udata, 1);
}