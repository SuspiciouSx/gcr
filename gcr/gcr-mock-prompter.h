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

#if !defined (__GCR_INSIDE_HEADER__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> or <gcr/gcr-base.h> can be included directly."
#endif

#ifndef __GCR_MOCK_PROMPTER_H__
#define __GCR_MOCK_PROMPTER_H__

#include "gcr-system-prompter.h"

#include <glib-object.h>

G_BEGIN_DECLS

const gchar *        gcr_mock_prompter_start                     (void);

void                 gcr_mock_prompter_stop                      (void);

gboolean             gcr_mock_prompter_is_prompting              (void);

guint                gcr_mock_prompter_get_delay_msec            (void);

void                 gcr_mock_prompter_set_delay_msec            (guint delay_msec);

void                 gcr_mock_prompter_expect_confirm_ok         (const gchar *first_property_name,
                                                                  ...);

void                 gcr_mock_prompter_expect_confirm_cancel     (void);

void                 gcr_mock_prompter_expect_password_ok        (const gchar *password,
                                                                  const gchar *first_property_name,
                                                                  ...);

void                 gcr_mock_prompter_expect_password_cancel    (void);

gboolean             gcr_mock_prompter_is_expecting              (void);

G_END_DECLS

#endif /* __GCR_MOCK_PROMPTER_H__ */
