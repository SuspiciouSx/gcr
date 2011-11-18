/*
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

#include "gcr-certificate-request.h"
#include "gcr-enum-types-base.h"
#include "gcr-oids.h"
#include "gcr-subject-public-key.h"

#include <egg/egg-asn1x.h>
#include <egg/egg-asn1-defs.h>
#include <egg/egg-dn.h>

#include <glib/gi18n-lib.h>

#define GCR_CERTIFICATE_REQUEST_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_REQUEST, GcrCertificateRequestClass))
#define GCR_IS_CERTIFICATE_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_REQUEST))
#define GCR_CERTIFICATE_REQUEST_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_REQUEST, GcrCertificateRequestClass))

typedef struct _GcrCertificateRequestClass GcrCertificateRequestClass;

struct _GcrCertificateRequest {
	GObject parent;

	GckObject *private_key;
	GNode *asn;
};

struct _GcrCertificateRequestClass {
	GObjectClass parent_class;
};

enum {
	PROP_0,
	PROP_FORMAT,
	PROP_PRIVATE_KEY
};

/* Forward declarations */
G_DEFINE_TYPE (GcrCertificateRequest, gcr_certificate_request, G_TYPE_OBJECT);

static void
gcr_certificate_request_init (GcrCertificateRequest *self)
{

}

static void
gcr_certificate_request_constructed (GObject *obj)
{
	GcrCertificateRequest *self = GCR_CERTIFICATE_REQUEST (obj);
	GNode *version;

	G_OBJECT_CLASS (gcr_certificate_request_parent_class)->constructed (obj);

	self->asn = egg_asn1x_create (pkix_asn1_tab, "pkcs-10-CertificationRequest");
	g_return_if_fail (self->asn != NULL);

	/* Setup the version */
	version = egg_asn1x_node (self->asn, "certificationRequestInfo", "version", NULL);
	egg_asn1x_set_integer_as_ulong (version, 0);
}

static void
gcr_certificate_request_finalize (GObject *obj)
{
	GcrCertificateRequest *self = GCR_CERTIFICATE_REQUEST (obj);

	egg_asn1x_destroy (self->asn);

	G_OBJECT_CLASS (gcr_certificate_request_parent_class)->finalize (obj);
}

static void
gcr_certificate_request_set_property (GObject *obj,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	GcrCertificateRequest *self = GCR_CERTIFICATE_REQUEST (obj);
	GcrCertificateRequestFormat format;

	switch (prop_id) {
	case PROP_PRIVATE_KEY:
		g_return_if_fail (self->private_key == NULL);
		self->private_key = g_value_dup_object (value);
		g_return_if_fail (GCK_IS_OBJECT (self->private_key));
		break;
	case PROP_FORMAT:
		format = g_value_get_enum (value);
		g_return_if_fail (format == GCR_CERTIFICATE_REQUEST_PKCS10);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_certificate_request_get_property (GObject *obj,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	GcrCertificateRequest *self = GCR_CERTIFICATE_REQUEST (obj);

	switch (prop_id) {
	case PROP_PRIVATE_KEY:
		g_value_set_object (value, self->private_key);
		break;
	case PROP_FORMAT:
		g_value_set_enum (value, GCR_CERTIFICATE_REQUEST_PKCS10);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
gcr_certificate_request_class_init (GcrCertificateRequestClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	_gcr_oids_init ();

	gobject_class->constructed = gcr_certificate_request_constructed;
	gobject_class->finalize = gcr_certificate_request_finalize;
	gobject_class->set_property = gcr_certificate_request_set_property;
	gobject_class->get_property = gcr_certificate_request_get_property;

	g_object_class_install_property (gobject_class, PROP_PRIVATE_KEY,
	            g_param_spec_object ("private-key", "Private key", "Private key for request",
	                                 GCK_TYPE_OBJECT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_FORMAT,
	              g_param_spec_enum ("format", "Format", "Format of certificate request",
	                                 GCR_TYPE_CERTIFICATE_REQUEST_FORMAT, GCR_CERTIFICATE_REQUEST_PKCS10,
	                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/**
 * gcr_certificate_request_prepare:
 * @format: the format for the certificate request
 * @private_key: the private key the the certificate is being requested for
 *
 * xxx
 *
 * Returns: (transfer full): a new #GcrCertificate request
 */
GcrCertificateRequest *
gcr_certificate_request_prepare (GcrCertificateRequestFormat format,
                                 GckObject *private_key)
{
	g_return_val_if_fail (format == GCR_CERTIFICATE_REQUEST_PKCS10, NULL);
	g_return_val_if_fail (GCK_IS_OBJECT (private_key), NULL);

	return g_object_new (GCR_TYPE_CERTIFICATE_REQUEST,
	                     "format", format,
	                     "private-key", private_key,
	                     NULL);
}

void
gcr_certificate_request_set_cn (GcrCertificateRequest *self,
                                const gchar *cn)
{
	GNode *subject;
	GNode *dn;

	g_return_if_fail (GCR_IS_CERTIFICATE_REQUEST (self));
	g_return_if_fail (cn != NULL);

	subject = egg_asn1x_node (self->asn, "certificationRequestInfo", "subject", NULL);
	dn = egg_asn1x_node (subject, "rdnSequence", NULL);

	/* TODO: we shouldn't really be clearing this, but replacing CN */
	egg_asn1x_set_choice (subject, dn);
	egg_asn1x_clear (dn);
	egg_dn_add_string_part (dn, GCR_OID_NAME_CN, cn);
}


static EggBytes *
prepare_to_be_signed (GcrCertificateRequest *self)
{
	GNode *node;

	node = egg_asn1x_node (self->asn, "certificationRequestInfo", NULL);
	return egg_asn1x_encode (node, NULL);
}

static gboolean
prepare_subject_public_key_and_mechanism (GcrCertificateRequest *self,
                                          GNode *subject_public_key,
                                          GQuark *algorithm,
                                          GckMechanism *mechanism,
                                          GError **error)
{
	EggBytes *encoded;
	GNode *node;
	GQuark oid;

	g_assert (algorithm != NULL);
	g_assert (mechanism != NULL);

	encoded = egg_asn1x_encode (subject_public_key, NULL);
	g_return_val_if_fail (encoded != NULL, FALSE);

	node = egg_asn1x_node (subject_public_key, "algorithm", "algorithm", NULL);
	oid = egg_asn1x_get_oid_as_quark (node);

	memset (mechanism, 0, sizeof (GckMechanism));
	if (oid == GCR_OID_PKIX1_RSA) {
		mechanism->type = CKM_SHA1_RSA_PKCS;
		*algorithm = GCR_OID_PKIX1_SHA1_WITH_RSA;

	} else if (oid == GCR_OID_PKIX1_DSA) {
		mechanism->type = CKM_DSA_SHA1;
		*algorithm = GCR_OID_PKIX1_SHA1_WITH_DSA;

	} else {
		egg_bytes_unref (encoded);
		g_set_error (error, GCR_DATA_ERROR, GCR_ERROR_UNRECOGNIZED,
		             _("Unsupported key type for certificate request"));
		return FALSE;
	}

	node = egg_asn1x_node (self->asn, "certificationRequestInfo", "subjectPKInfo", NULL);
	if (!egg_asn1x_set_element_raw (node, encoded))
		g_return_val_if_reached (FALSE);

	egg_bytes_unref (encoded);
	return TRUE;
}

static void
encode_take_signature_into_request (GcrCertificateRequest *self,
                                    GQuark algorithm,
                                    GNode *subject_public_key,
                                    guchar *result,
                                    gsize n_result)
{
	EggBytes *data;
	GNode *params;
	GNode *node;

	node = egg_asn1x_node (self->asn, "signature", NULL);
	egg_asn1x_take_bits_as_raw (node, egg_bytes_new_take (result, n_result), n_result * 8);

	node = egg_asn1x_node (self->asn, "signatureAlgorithm", "algorithm", NULL);
	egg_asn1x_set_oid_as_quark (node, algorithm);

	node = egg_asn1x_node (self->asn, "signatureAlgorithm", "parameters", NULL);
	params = egg_asn1x_node (subject_public_key, "algorithm", "parameters", NULL);
	data = egg_asn1x_encode (params, NULL);
	egg_asn1x_set_element_raw (node, data);
	egg_bytes_unref (data);
}

gboolean
gcr_certificate_request_complete (GcrCertificateRequest *self,
                                  GCancellable *cancellable,
                                  GError **error)
{
	GNode *subject_public_key;
	GckMechanism mechanism = { 0, };
	GQuark algorithm = 0;
	EggBytes *tbs;
	GckSession *session;
	guchar *signature;
	gsize n_signature;
	gboolean ret;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_REQUEST (self), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	subject_public_key = _gcr_subject_public_key_load (self->private_key,
	                                                   cancellable, error);
	if (subject_public_key == NULL)
		return FALSE;

	ret = prepare_subject_public_key_and_mechanism (self, subject_public_key,
	                                                &algorithm, &mechanism, error);

	if (!ret) {
		egg_asn1x_destroy (subject_public_key);
		return FALSE;
	}

	tbs = prepare_to_be_signed (self);
	session = gck_object_get_session (self->private_key);
	signature = gck_session_sign_full (session, self->private_key, &mechanism,
	                                   egg_bytes_get_data (tbs),
	                                   egg_bytes_get_size (tbs),
	                                   &n_signature, cancellable, error);
	g_object_unref (session);
	egg_bytes_unref (tbs);

	if (!signature) {
		egg_asn1x_destroy (subject_public_key);
		return FALSE;
	}

	encode_take_signature_into_request (self, algorithm, subject_public_key,
	                                    signature, n_signature);
	egg_asn1x_destroy (subject_public_key);
	return TRUE;
}

typedef struct {
	GcrCertificateRequest *request;
	GCancellable *cancellable;
	GQuark algorithm;
	GNode *subject_public_key;
	GckMechanism mechanism;
	GckSession *session;
	EggBytes *tbs;
} CompleteClosure;

static void
complete_closure_free (gpointer data)
{
	CompleteClosure *closure = data;
	egg_asn1x_destroy (closure->subject_public_key);
	g_clear_object (&closure->request);
	g_clear_object (&closure->cancellable);
	g_clear_object (&closure->session);
	if (closure->tbs)
		egg_bytes_unref (closure->tbs);
	g_free (closure);
}

static void
on_certificate_request_signed (GObject *source,
                               GAsyncResult *result,
                               gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	CompleteClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	guchar *signature;
	gsize n_signature;

	signature = gck_session_sign_finish (closure->session, result, &n_signature, &error);
	if (result == NULL) {
		encode_take_signature_into_request (closure->request,
		                                    closure->algorithm,
		                                    closure->subject_public_key,
		                                    signature, n_signature);

	} else {
		g_simple_async_result_take_error (res, error);
	}

	g_simple_async_result_complete (res);
	g_object_unref (res);
}

static void
on_subject_public_key_loaded (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	CompleteClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	closure->subject_public_key = _gcr_subject_public_key_load_finish (result, &error);
	if (error == NULL) {
		prepare_subject_public_key_and_mechanism (closure->request,
		                                          closure->subject_public_key,
		                                          &closure->algorithm,
		                                          &closure->mechanism,
		                                          &error);
	}

	if (error != NULL) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);

	} else {
		closure->tbs = prepare_to_be_signed (closure->request);
		gck_session_sign_async (closure->session,
		                        closure->request->private_key,
		                        &closure->mechanism,
		                        egg_bytes_get_data (closure->tbs),
		                        egg_bytes_get_size (closure->tbs),
		                        closure->cancellable,
		                        on_certificate_request_signed,
		                        g_object_ref (res));
	}

	g_object_unref (res);
}

void
gcr_certificate_request_complete_async (GcrCertificateRequest *self,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
	GSimpleAsyncResult *res;
	CompleteClosure *closure;

	g_return_if_fail (GCR_IS_CERTIFICATE_REQUEST (self));
	g_return_if_fail (cancellable == NULL || G_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 gcr_certificate_request_complete_async);
	closure = g_new0 (CompleteClosure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->session = gck_object_get_session (self->private_key);
	closure->request = g_object_ref (self);
	g_simple_async_result_set_op_res_gpointer (res, closure, complete_closure_free);

	_gcr_subject_public_key_load_async (self->private_key, cancellable,
	                                    on_subject_public_key_loaded,
	                                    g_object_ref (res));

	g_object_unref (res);
}

/**
 *
 */
gboolean
gcr_certificate_request_complete_finish (GcrCertificateRequest *self,
                                         GAsyncResult *result,
                                         GError **error)
{
	g_return_val_if_fail (GCR_IS_CERTIFICATE_REQUEST (self), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      gcr_certificate_request_complete_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

/**
 * gcr_certificate_request_encode:
 * @self: a certificate request
 * @length: location to place length of returned data
 *
 * Encode the certificate request. It must have been completed with
 * gcr_certificate_request_complete() or gcr_certificate_request_complete_async()
 *
 * The output is a DER encoded certificate request.
 *
 * Returns: (transfer full) (array length=length): the encoded certificate request
 */
guchar *
gcr_certificate_request_encode (GcrCertificateRequest *self,
                                gsize *length)
{
	EggBytes *bytes;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_REQUEST (self), NULL);
	g_return_val_if_fail (length != NULL, NULL);

	bytes = egg_asn1x_encode (self->asn, NULL);
	if (bytes == NULL) {
		g_warning ("couldn't encode certificate request: %s",
		           egg_asn1x_message (self->asn));
		return NULL;
	}

	*length = egg_bytes_get_size (bytes);
	return g_byte_array_free (egg_bytes_unref_to_array (bytes), FALSE);
}
