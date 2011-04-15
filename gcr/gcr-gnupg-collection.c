/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "gcr-colons.h"
#include "gcr-collection.h"
#include "gcr-gnupg-collection.h"
#include "gcr-gnupg-key.h"
#include "gcr-internal.h"
#include "gcr-util.h"

#include "egg/egg-spawn.h"

#include <sys/wait.h>
#include <string.h>

enum {
	PROP_0,
	PROP_DIRECTORY,
	PROP_MAIN_CONTEXT
};

struct _GcrGnupgCollectionPrivate {
	GHashTable *items;
	gchar *directory;
};

static void _gcr_collection_iface (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GcrGnupgCollection, _gcr_gnupg_collection, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, _gcr_collection_iface)
);

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
_gcr_gnupg_collection_init (GcrGnupgCollection *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, GCR_TYPE_GNUPG_COLLECTION,
	                                        GcrGnupgCollectionPrivate);

	self->pv->items = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                         g_free, g_object_unref);
}

static void
_gcr_gnupg_collection_set_property (GObject *obj, guint prop_id, const GValue *value,
                                    GParamSpec *pspec)
{
	GcrGnupgCollection *self = GCR_GNUPG_COLLECTION (obj);

	switch (prop_id) {
	case PROP_DIRECTORY:
		g_return_if_fail (!self->pv->directory);
		self->pv->directory = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
_gcr_gnupg_collection_get_property (GObject *obj, guint prop_id, GValue *value,
                                    GParamSpec *pspec)
{
	GcrGnupgCollection *self = GCR_GNUPG_COLLECTION (obj);

	switch (prop_id) {
	case PROP_DIRECTORY:
		g_value_set_string (value, self->pv->directory);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}
static void
_gcr_gnupg_collection_dispose (GObject *obj)
{
	GcrGnupgCollection *self = GCR_GNUPG_COLLECTION (obj);

	g_hash_table_remove_all (self->pv->items);

	G_OBJECT_CLASS (_gcr_gnupg_collection_parent_class)->dispose (obj);
}

static void
_gcr_gnupg_collection_finalize (GObject *obj)
{
	GcrGnupgCollection *self = GCR_GNUPG_COLLECTION (obj);

	g_assert (self->pv->items);
	g_assert (g_hash_table_size (self->pv->items) == 0);
	g_hash_table_destroy (self->pv->items);
	self->pv->items = NULL;

	g_free (self->pv->directory);
	self->pv->directory = NULL;

	G_OBJECT_CLASS (_gcr_gnupg_collection_parent_class)->finalize (obj);
}

static void
_gcr_gnupg_collection_class_init (GcrGnupgCollectionClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = _gcr_gnupg_collection_get_property;
	gobject_class->set_property = _gcr_gnupg_collection_set_property;
	gobject_class->dispose = _gcr_gnupg_collection_dispose;
	gobject_class->finalize = _gcr_gnupg_collection_finalize;

	g_object_class_install_property (gobject_class, PROP_DIRECTORY,
	           g_param_spec_string ("directory", "Directory", "Gnupg Directory",
	                                NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (gobject_class, sizeof (GcrGnupgCollectionPrivate));
	_gcr_initialize ();
}

static guint
gcr_gnupg_collection_real_get_length (GcrCollection *coll)
{
	GcrGnupgCollection *self = GCR_GNUPG_COLLECTION (coll);
	return g_hash_table_size (self->pv->items);
}

static GList*
gcr_gnupg_collection_real_get_objects (GcrCollection *coll)
{
	GcrGnupgCollection *self = GCR_GNUPG_COLLECTION (coll);
	return g_hash_table_get_keys (self->pv->items);
}

static void
_gcr_collection_iface (GcrCollectionIface *iface)
{
	iface->get_length = gcr_gnupg_collection_real_get_length;
	iface->get_objects = gcr_gnupg_collection_real_get_objects;
}

GcrCollection*
_gcr_gnupg_collection_new (const gchar *directory)
{
	return g_object_new (GCR_TYPE_GNUPG_COLLECTION,
	                     "directory", directory,
	                     NULL);
}

typedef struct {
	GcrGnupgCollection *collection;
	GPtrArray *dataset;
	guint spawn_sig;
	guint child_sig;
	GPid gnupg_pid;
	GString *out_data;
	GString *err_data;
	GHashTable *difference;
} GcrGnupgCollectionLoad;

static void
_gcr_gnupg_collection_load_free (gpointer data)
{
	GcrGnupgCollectionLoad *load = data;
	g_assert (load);

	g_ptr_array_unref (load->dataset);
	g_string_free (load->err_data, TRUE);
	g_string_free (load->out_data, TRUE);
	g_hash_table_destroy (load->difference);
	g_object_unref (load->collection);

	if (load->spawn_sig)
		g_source_remove (load->spawn_sig);
	if (load->child_sig)
		g_source_remove (load->child_sig);
	if (load->gnupg_pid) {
		kill (load->gnupg_pid, SIGTERM);
		g_spawn_close_pid (load->gnupg_pid);
	}
	g_slice_free (GcrGnupgCollectionLoad, load);
}

static void
process_dataset_as_key (GcrGnupgCollectionLoad *load)
{
	const gchar *keyid;
	GPtrArray *dataset;
	GcrGnupgKey *key;

	g_assert (load->dataset->len);

	dataset = load->dataset;
	load->dataset = g_ptr_array_new_with_free_func (_gcr_colons_free);

	keyid = _gcr_gnupg_key_get_keyid_for_colons (dataset);
	if (keyid) {
		/* Note that we've seen this keyid */
		g_hash_table_remove (load->difference, keyid);

		key = g_hash_table_lookup (load->collection->pv->items, keyid);

		/* Already have this key, just update */
		if (key) {
			_gcr_gnupg_key_set_dataset (key, dataset);

		/* Add a new key */
		} else {
			key = _gcr_gnupg_key_new (dataset);
			g_hash_table_insert (load->collection->pv->items, g_strdup (keyid), key);
			gcr_collection_emit_added (GCR_COLLECTION (load->collection), G_OBJECT (key));
		}

	} else {
		g_warning ("parsed gnupg data had no keyid");
	}

	g_ptr_array_unref (dataset);
}

static void
on_line_parse_output (const gchar *line, gpointer user_data)
{
	GcrGnupgCollectionLoad *load = user_data;
	GcrColons *colons;
	GQuark schema;

	colons = _gcr_colons_parse (line, -1);
	if (!colons) {
		g_warning ("invalid gnupg output line: %s", line);
		return;
	}

	schema = _gcr_colons_get_schema (colons);
	if (schema == GCR_COLONS_SCHEMA_PUB) {
		if (load->dataset->len)
			process_dataset_as_key (load);
		g_assert (!load->dataset->len);
		g_ptr_array_add (load->dataset, colons);
		colons = NULL;
	} else if (schema == GCR_COLONS_SCHEMA_UID) {
		if (load->dataset->len) {
			g_ptr_array_add (load->dataset, colons);
			colons = NULL;
		}
	}

	if (colons != NULL)
		_gcr_colons_free (colons);
}


static gboolean
on_spawn_standard_output (int fd, gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GcrGnupgCollectionLoad *load = g_simple_async_result_get_op_res_gpointer (res);
	gchar buffer[1024];
	gssize ret;

	ret = egg_spawn_read_output (fd, buffer, sizeof (buffer));
	if (ret < 0) {
		g_warning ("couldn't read output data from prompt process");
		return FALSE;
	}

	g_string_append_len (load->out_data, buffer, ret);
	_gcr_util_parse_lines (load->out_data, FALSE, on_line_parse_output, load);

	return (ret > 0);
}

static void
on_line_print_error (const gchar *line, gpointer unused)
{
	g_printerr ("%s\n", line);
}

static gboolean
on_spawn_standard_error (int fd, gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GcrGnupgCollectionLoad *load = g_simple_async_result_get_op_res_gpointer (res);
	gchar buffer[1024];
	gssize ret;

	ret = egg_spawn_read_output (fd, buffer, sizeof (buffer));
	if (ret < 0) {
		g_warning ("couldn't read error data from prompt process");
		return FALSE;
	}

	g_string_append_len (load->err_data, buffer, ret);
	_gcr_util_parse_lines (load->err_data, FALSE, on_line_print_error, NULL);

	return ret > 0;
}

static void
on_each_difference_remove (gpointer key, gpointer value, gpointer user_data)
{
	GcrGnupgCollection *self = GCR_GNUPG_COLLECTION (user_data);
	GObject *object;

	object = g_hash_table_lookup (self->pv->items, key);
	if (object != NULL) {
		g_object_ref (object);
		g_hash_table_remove (self->pv->items, key);
		gcr_collection_emit_removed (GCR_COLLECTION (self), object);
		g_object_unref (object);
	}
}

static void
on_spawn_completed (gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GcrGnupgCollectionLoad *load = g_simple_async_result_get_op_res_gpointer (res);

	/* Should be the last call we receive */
	g_assert (load->spawn_sig != 0);
	load->spawn_sig = 0;

	/* Print out any remaining errors */
	_gcr_util_parse_lines (load->err_data, TRUE, on_line_print_error, NULL);

	/* Process any remaining output */
	_gcr_util_parse_lines (load->out_data, TRUE, on_line_parse_output, load);

	/* Process last bit as a key, if any */
	if (load->dataset->len)
		process_dataset_as_key (load);

	/* Remove any keys that we still have in the difference */
	g_hash_table_foreach (load->difference, on_each_difference_remove, load->collection);

	g_simple_async_result_complete (res);
}

static void
on_child_exited (GPid pid, gint status, gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GcrGnupgCollectionLoad *load = g_simple_async_result_get_op_res_gpointer (res);
	gint code;

	g_return_if_fail (pid == load->gnupg_pid);

	g_spawn_close_pid (load->gnupg_pid);
	load->gnupg_pid = 0;
	load->child_sig = 0;

	if (WIFEXITED (status)) {
		code = WEXITSTATUS (status);
		if (code != 0) {
			g_message ("gnupg process exited with failure code: %d", code);
		} else if (WIFSIGNALED (status)) {
			code = WTERMSIG (status);
			g_message ("gnupg process was killed with signal: %d", code);
		}
	}
}

static EggSpawnCallbacks spawn_callbacks = {
	NULL,
	on_spawn_standard_output,
	on_spawn_standard_error,
	on_spawn_completed,
	g_object_unref,
	NULL
};

static void
on_each_item_add_keyid_to_difference (gpointer key, gpointer value, gpointer user_data)
{
	GHashTable *difference = user_data;
	g_hash_table_insert (difference, key, key);
}

void
_gcr_gnupg_collection_load_async (GcrGnupgCollection *self, GCancellable *cancellable,
                                  GAsyncReadyCallback callback, gpointer user_data)
{
	GSimpleAsyncResult *res;
	GcrGnupgCollectionLoad *load;
	GError *error = NULL;
	GPtrArray *argv;

	g_return_if_fail (GCR_IS_GNUPG_COLLECTION (self));

	/* Not yet implemented */
	g_return_if_fail (cancellable == NULL);

	argv = g_ptr_array_new ();
	g_ptr_array_add (argv, GPG_EXECUTABLE);
	g_ptr_array_add (argv, "--list-keys");
	g_ptr_array_add (argv, "--fixed-list-mode");
	g_ptr_array_add (argv, "--with-colons");
	if (self->pv->directory) {
		g_ptr_array_add (argv, "--homedir");
		g_ptr_array_add (argv, (gpointer)self->pv->directory);
	}
	g_ptr_array_add (argv, NULL);

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 _gcr_gnupg_collection_load_async);

	load = g_slice_new0 (GcrGnupgCollectionLoad);
	load->dataset = g_ptr_array_new_with_free_func (_gcr_colons_free);
	load->err_data = g_string_sized_new (128);
	load->out_data = g_string_sized_new (1024);
	load->difference = g_hash_table_new (g_str_hash, g_str_equal);
	load->collection = g_object_ref (self);
	g_hash_table_foreach (self->pv->items, on_each_item_add_keyid_to_difference,
	                      load->difference);

	g_simple_async_result_set_op_res_gpointer (res, load,
	                                           _gcr_gnupg_collection_load_free);

	load->spawn_sig = egg_spawn_async_with_callbacks (self->pv->directory,
	                                                  (gchar**)argv->pdata, NULL,
	                                                  G_SPAWN_DO_NOT_REAP_CHILD,
	                                                  &load->gnupg_pid,
	                                                  &spawn_callbacks,
	                                                  g_object_ref (res),
	                                                  NULL, &error);

	if (error) {
		g_simple_async_result_set_from_error (res, error);
		g_simple_async_result_complete_in_idle (res);
	} else {
		load->child_sig = g_child_watch_add_full (G_PRIORITY_DEFAULT,
		                                          load->gnupg_pid,
		                                          on_child_exited,
		                                          g_object_ref (res),
		                                          g_object_unref);
	}

	g_object_unref (res);
}

gboolean
_gcr_gnupg_collection_load_finish (GcrGnupgCollection *self, GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (GCR_IS_GNUPG_COLLECTION (self), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      _gcr_gnupg_collection_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}
