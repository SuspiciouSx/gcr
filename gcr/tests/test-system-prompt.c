/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
   Copyright (C) 2011 Collabora Ltd

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gcr/gcr-base.h"
#include "gcr/gcr-mock-prompter.h"

#include "egg/egg-testing.h"

#include <glib.h>

typedef struct {
	const gchar *prompter_name;
} Test;

static void
setup (Test *test,
       gconstpointer unused)
{
	test->prompter_name = gcr_mock_prompter_start ();
}

static void
teardown (Test *test,
          gconstpointer unused)
{
	gcr_mock_prompter_stop ();
}

static void
test_open_prompt (Test *test,
                  gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;
	gchar *bus_name;

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	g_object_get (prompt, "bus-name", &bus_name, NULL);
	g_assert_cmpstr (bus_name, ==, test->prompter_name);

	ret = gcr_system_prompt_close (GCR_SYSTEM_PROMPT (prompt), NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	g_free (bus_name);
	g_object_unref (prompt);
}

static void
test_open_failure (Test *test,
                   gconstpointer unused)
{
	GcrPrompt *prompt;
	GDBusConnection *connection;
	GError *error = NULL;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	g_assert_no_error (error);

	/* Try to open a prompt where no prompter is running */

	prompt = gcr_system_prompt_open_for_prompter (g_dbus_connection_get_unique_name (connection),
	                                              0, NULL, &error);
	g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
	g_assert (prompt == NULL);

	g_object_unref (connection);
}

static void
test_prompt_password (Test *test,
                      gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	const gchar *password;

	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	password = gcr_prompt_password_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (password, ==, "booo");

	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

static void
test_password_in_exchange (Test *test,
                           gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	GcrSecretExchange *exchange;

	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	gcr_prompt_password_run (prompt, NULL, &error);
	g_assert_no_error (error);

	g_object_get (prompt, "secret-exchange", &exchange, NULL);
	g_assert (GCR_IS_SECRET_EXCHANGE (exchange));
	g_assert_cmpstr (gcr_secret_exchange_get_secret (exchange, NULL), ==, "booo");

	g_object_unref (exchange);
	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

static void
test_password_custom_exchange (Test *test,
                               gconstpointer unused)
{
	GcrSystemPrompt *prompt;
	GError *error = NULL;
	GcrSecretExchange *exchange;
	const gchar *password;

	exchange = gcr_secret_exchange_new (NULL);
	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	prompt = g_initable_new (GCR_TYPE_SYSTEM_PROMPT, NULL, &error,
	                         "timeout-seconds", 0,
	                         "bus-name", test->prompter_name,
	                         "secret-exchange", exchange,
	                         NULL);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	password = gcr_prompt_password_run (GCR_PROMPT (prompt), NULL, &error);
	g_assert_cmpstr (password, ==, "booo");
	g_assert_no_error (error);

	password = gcr_secret_exchange_get_secret (exchange, NULL);
	g_assert_cmpstr (password, ==, "booo");

	g_object_unref (exchange);
	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

static void
on_async_result (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GAsyncResult **ret = user_data;
	*ret = g_object_ref (result);
	egg_test_wait_stop ();
}

static void
test_async_password (Test *test,
                     gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GcrPrompt *prompt;
	GError *error = NULL;
	const gchar *password;

	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	gcr_system_prompt_open_for_prompter_async (test->prompter_name, 0, NULL,
	                                           on_async_result, &result);
	g_assert (result == NULL);
	egg_test_wait ();

	g_assert (result != NULL);
	prompt = gcr_system_prompt_open_finish (result, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_clear_object (&result);

	gcr_prompt_password_async (prompt, NULL,
	                           on_async_result, &result);
	g_assert (result == NULL);
	egg_test_wait ();

	password = gcr_prompt_password_finish (prompt, result, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (password, ==, "booo");
	g_clear_object (&result);

	g_object_unref (prompt);
}

static void
test_prompt_confirm (Test *test,
                     gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;

	gcr_mock_prompter_expect_confirm_ok (NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

static void
test_async_confirm (Test *test,
                    gconstpointer unused)
{
	GAsyncResult *result = NULL;
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean confirm;

	gcr_mock_prompter_expect_confirm_ok (NULL);

	gcr_system_prompt_open_for_prompter_async (test->prompter_name, 0, NULL,
	                                           on_async_result, &result);
	g_assert (result == NULL);
	egg_test_wait ();

	g_assert (result != NULL);
	prompt = gcr_system_prompt_open_finish (result, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));
	g_clear_object (&result);

	gcr_prompt_confirm_async (prompt, NULL, on_async_result, &result);
	g_assert (result == NULL);
	egg_test_wait ();

	confirm = gcr_prompt_confirm_finish (prompt, result, &error);
	g_assert_no_error (error);
	g_assert (confirm == TRUE);
	g_clear_object (&result);

	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

static void
test_cancel_password (Test *test,
                      gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	const gchar *password;

	gcr_mock_prompter_expect_password_cancel ();

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	password = gcr_prompt_password_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (password, ==, NULL);

	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

static void
test_cancel_confirm (Test *test,
                     gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;

	gcr_mock_prompter_expect_confirm_cancel ();

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == FALSE);

	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

static void
test_prompt_properties (Test *test,
                        gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	gboolean ret;

	gcr_mock_prompter_expect_confirm_ok ("title", "My Title",
	                                     "description", "My Description",
	                                     "warning", "My Warning",
	                                     "message", "My Message",
	                                     "caller-window", "01010",
	                                     "choice-label", "My Choice",
	                                     "choice-chosen", TRUE,
	                                     "password-new", TRUE,
	                                     "password-strength", 0,
	                                     "continue-label", "My Continue",
	                                     "cancel-label", "My Cancel",
	                                     NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	g_object_set (prompt,
	              "title", "Other Title",
	              "choice-label", "Other Choice",
	              "description", "Other Description",
	              "message", "Other Message",
	              "caller-window", "01012",
	              "warning", "Other Warning",
	              "password-new", FALSE,
	              "choice-chosen", TRUE,
	              "continue-label", "Other Continue",
	              "cancel-label", "Other Cancel",
	              NULL);

	g_assert_cmpstr (gcr_prompt_get_title (prompt), ==, "Other Title");
	g_assert_cmpstr (gcr_prompt_get_choice_label (prompt), ==, "Other Choice");
	g_assert_cmpstr (gcr_prompt_get_description (prompt), ==, "Other Description");
	g_assert_cmpstr (gcr_prompt_get_message (prompt), ==, "Other Message");
	g_assert_cmpstr (gcr_prompt_get_caller_window (prompt), ==, "01012");
	g_assert_cmpstr (gcr_prompt_get_warning (prompt), ==, "Other Warning");
	g_assert_cmpstr (gcr_prompt_get_continue_label (prompt), ==, "Other Continue");
	g_assert_cmpstr (gcr_prompt_get_cancel_label (prompt), ==, "Other Cancel");
	g_assert (gcr_prompt_get_password_new (prompt) == FALSE);
	g_assert (gcr_prompt_get_choice_chosen (prompt) == TRUE);

	gcr_prompt_set_title (prompt, "My Title");
	gcr_prompt_set_choice_label (prompt, "My Choice");
	gcr_prompt_set_description (prompt, "My Description");
	gcr_prompt_set_message (prompt, "My Message");
	gcr_prompt_set_caller_window (prompt, "01010");
	gcr_prompt_set_warning (prompt, "My Warning");
	gcr_prompt_set_continue_label (prompt, "My Continue");
	gcr_prompt_set_cancel_label (prompt, "My Cancel");
	gcr_prompt_set_password_new (prompt, TRUE);
	gcr_prompt_set_choice_chosen (prompt, TRUE);

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	g_assert (gcr_prompt_get_choice_chosen (prompt) == TRUE);
	g_assert_cmpint (gcr_prompt_get_password_strength (prompt), ==, 0);

	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

static void
test_prompt_properties_unset (Test *test,
                              gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	g_assert_cmpstr (gcr_prompt_get_title (prompt), ==, "");
	g_assert_cmpstr (gcr_prompt_get_choice_label (prompt), ==, NULL);
	g_assert_cmpstr (gcr_prompt_get_description (prompt), ==, "");
	g_assert_cmpstr (gcr_prompt_get_message (prompt), ==, "");
	g_assert_cmpstr (gcr_prompt_get_caller_window (prompt), ==, NULL);
	g_assert_cmpstr (gcr_prompt_get_warning (prompt), ==, NULL);
	g_assert_cmpstr (gcr_prompt_get_continue_label (prompt), ==, "Continue");
	g_assert_cmpstr (gcr_prompt_get_cancel_label (prompt), ==, "Cancel");
	g_assert (gcr_prompt_get_password_new (prompt) == FALSE);
	g_assert (gcr_prompt_get_choice_chosen (prompt) == FALSE);
	g_assert_cmpint (gcr_prompt_get_password_strength (prompt), ==, 0);

	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}


static void
test_prompt_properties_reset (Test *test,
                              gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);

	g_object_set (prompt,
	              "title", "Other Title",
	              "choice-label", "Other Choice",
	              "description", "Other Description",
	              "message", "Other Message",
	              "caller-window", "01012",
	              "warning", "Other Warning",
	              "password-new", FALSE,
	              "choice-chosen", TRUE,
	              "continue-label", "Other Continue",
	              "cancel-label", "Other Cancel",
	              NULL);

	g_assert_cmpstr (gcr_prompt_get_title (prompt), ==, "Other Title");
	g_assert_cmpstr (gcr_prompt_get_choice_label (prompt), ==, "Other Choice");
	g_assert_cmpstr (gcr_prompt_get_description (prompt), ==, "Other Description");
	g_assert_cmpstr (gcr_prompt_get_message (prompt), ==, "Other Message");
	g_assert_cmpstr (gcr_prompt_get_caller_window (prompt), ==, "01012");
	g_assert_cmpstr (gcr_prompt_get_warning (prompt), ==, "Other Warning");
	g_assert_cmpstr (gcr_prompt_get_continue_label (prompt), ==, "Other Continue");
	g_assert_cmpstr (gcr_prompt_get_cancel_label (prompt), ==, "Other Cancel");
	g_assert (gcr_prompt_get_password_new (prompt) == FALSE);
	g_assert (gcr_prompt_get_choice_chosen (prompt) == TRUE);

	gcr_prompt_reset (prompt);

	g_assert_cmpstr (gcr_prompt_get_title (prompt), ==, "");
	g_assert_cmpstr (gcr_prompt_get_choice_label (prompt), ==, NULL);
	g_assert_cmpstr (gcr_prompt_get_description (prompt), ==, "");
	g_assert_cmpstr (gcr_prompt_get_message (prompt), ==, "");
	g_assert_cmpstr (gcr_prompt_get_caller_window (prompt), ==, NULL);
	g_assert_cmpstr (gcr_prompt_get_warning (prompt), ==, NULL);
	g_assert_cmpstr (gcr_prompt_get_continue_label (prompt), ==, "Continue");
	g_assert_cmpstr (gcr_prompt_get_cancel_label (prompt), ==, "Cancel");
	g_assert (gcr_prompt_get_password_new (prompt) == FALSE);
	g_assert (gcr_prompt_get_choice_chosen (prompt) == FALSE);
	g_assert_cmpint (gcr_prompt_get_password_strength (prompt), ==, 0);

	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

static void
test_prompt_close (Test *test,
                   gconstpointer unused)
{
	GcrPrompt *prompt;
	GcrPrompt *prompt2;
	GError *error = NULL;
	gboolean ret;

	gcr_mock_prompter_expect_confirm_ok (NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	prompt2 = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_error (error, GCR_SYSTEM_PROMPT_ERROR, GCR_SYSTEM_PROMPT_IN_PROGRESS);
	g_clear_error (&error);
	g_assert (prompt2 == NULL);

	ret = gcr_prompt_confirm_run (prompt, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	prompt2 = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_error (error, GCR_SYSTEM_PROMPT_ERROR, GCR_SYSTEM_PROMPT_IN_PROGRESS);
	g_clear_error (&error);
	g_assert (prompt2 == NULL);

	gcr_system_prompt_close (GCR_SYSTEM_PROMPT (prompt), NULL, &error);
	g_assert_no_error (error);

	prompt2 = gcr_system_prompt_open_for_prompter (test->prompter_name, 1, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt2));

	g_object_unref (prompt);
	g_assert (!G_IS_OBJECT (prompt));

	g_object_unref (prompt2);
	egg_assert_not_object (prompt);
}

static void
test_close_cancels (Test *test,
                    gconstpointer unused)
{
	GcrPrompt *prompt;
	GError *error = NULL;
	const gchar *password = NULL;
	GAsyncResult *result = NULL;

	gcr_mock_prompter_set_delay_msec (3000);
	gcr_mock_prompter_expect_password_ok ("booo", NULL);

	prompt = gcr_system_prompt_open_for_prompter (test->prompter_name, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert (GCR_IS_SYSTEM_PROMPT (prompt));

	gcr_prompt_password_async (prompt, NULL, on_async_result, &result);

	gcr_system_prompt_close (GCR_SYSTEM_PROMPT (prompt), NULL, &error);
	g_assert_no_error (error);

	egg_test_wait ();

	password = gcr_prompt_password_finish (prompt, result, &error);
	g_assert_no_error (error);
	g_assert (password == NULL);
	g_clear_object (&result);

	g_object_unref (prompt);
	egg_assert_not_object (prompt);
}

int
main (int argc, char **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-system-prompt");

	g_test_add ("/gcr/system-prompt/open", Test, NULL, setup, test_open_prompt, teardown);
	g_test_add ("/gcr/system-prompt/open-failure", Test, NULL, setup, test_open_failure, teardown);
	g_test_add ("/gcr/system-prompt/password", Test, NULL, setup, test_prompt_password, teardown);
	g_test_add ("/gcr/system-prompt/password-async", Test, NULL, setup, test_async_password, teardown);
	g_test_add ("/gcr/system-prompt/password-cancel", Test, NULL, setup, test_cancel_password, teardown);
	g_test_add ("/gcr/system-prompt/password-in-exchange", Test, NULL, setup, test_password_in_exchange, teardown);
	g_test_add ("/gcr/system-prompt/password-custom-exchange", Test, NULL, setup, test_password_custom_exchange, teardown);
	g_test_add ("/gcr/system-prompt/confirm", Test, NULL, setup, test_prompt_confirm, teardown);
	g_test_add ("/gcr/system-prompt/confirm-async", Test, NULL, setup, test_async_confirm, teardown);
	g_test_add ("/gcr/system-prompt/confirm-cancel", Test, NULL, setup, test_cancel_confirm, teardown);
	g_test_add ("/gcr/system-prompt/properties", Test, NULL, setup, test_prompt_properties, teardown);
	g_test_add ("/gcr/system-prompt/properties-unset", Test, NULL, setup, test_prompt_properties_unset, teardown);
	g_test_add ("/gcr/system-prompt/properties-reset", Test, NULL, setup, test_prompt_properties_reset, teardown);
	g_test_add ("/gcr/system-prompt/close", Test, NULL, setup, test_prompt_close, teardown);
	g_test_add ("/gcr/system-prompt/close-cancels", Test, NULL, setup, test_close_cancels, teardown);

	return egg_tests_run_with_loop ();
}
