/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012 Red Hat, Inc.

   Red Hat Authors:
   Hans de Goede <hdegoede@redhat.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <glib-object.h>

#include "glib-compat.h"
#include "spice-session-priv.h"
#include "desktop-integration.h"

#include <glib/gi18n.h>

#define GNOME_SESSION_INHIBIT_AUTOMOUNT 16

/* ------------------------------------------------------------------ */
/* gobject glue                                                       */

#define SPICE_DESKTOP_INTEGRATION_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_DESKTOP_INTEGRATION, SpiceDesktopIntegrationPrivate))

struct _SpiceDesktopIntegrationPrivate {
#if defined(USE_GDBUS)
    GDBusProxy *gnome_session_proxy;
#else
    GObject *gnome_session_proxy; /* dummy */
#endif
    guint gnome_automount_inhibit_cookie;
};

G_DEFINE_TYPE(SpiceDesktopIntegration, spice_desktop_integration, G_TYPE_OBJECT);

/* ------------------------------------------------------------------ */
/* Gnome specific code                                                */

static void handle_dbus_call_error(const char *call, GError **_error)
{
    GError *error = *_error;
    const char *message = error->message;

    g_warning("Error calling '%s': %s", call, message);
    g_clear_error(_error);
}

static gboolean gnome_integration_init(SpiceDesktopIntegration *self)
{
    G_GNUC_UNUSED SpiceDesktopIntegrationPrivate *priv = self->priv;
    GError *error = NULL;
    gboolean success = TRUE;

#if defined(USE_GDBUS)
    gchar *name_owner = NULL;
    priv->gnome_session_proxy =
        g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                      G_DBUS_PROXY_FLAGS_NONE,
                                      NULL,
                                      "org.gnome.SessionManager",
                                      "/org/gnome/SessionManager",
                                      "org.gnome.SessionManager",
                                      NULL,
                                      &error);
    if (!error &&
        (name_owner = g_dbus_proxy_get_name_owner(priv->gnome_session_proxy)) == NULL) {
        g_clear_object(&priv->gnome_session_proxy);
        success = FALSE;
    }
    g_free(name_owner);
#else
    success = FALSE;
#endif

    if (error) {
        g_warning("Could not create org.gnome.SessionManager dbus proxy: %s",
                  error->message);
        g_clear_error(&error);
        return FALSE;
    }

    return success;
}

static void gnome_integration_inhibit_automount(SpiceDesktopIntegration *self)
{
    SpiceDesktopIntegrationPrivate *priv = self->priv;
    GError *error = NULL;
    G_GNUC_UNUSED const gchar *reason =
        _("Automounting has been inhibited for USB auto-redirecting");

    if (!priv->gnome_session_proxy)
        return;

    g_return_if_fail(priv->gnome_automount_inhibit_cookie == 0);

#if defined(USE_GDBUS)
    GVariant *v = g_dbus_proxy_call_sync(priv->gnome_session_proxy,
                "Inhibit",
                g_variant_new("(susu)",
                              g_get_prgname(),
                              0,
                              reason,
                              GNOME_SESSION_INHIBIT_AUTOMOUNT),
                G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (v)
        g_variant_get(v, "(u)", &priv->gnome_automount_inhibit_cookie);

    g_clear_pointer(&v, g_variant_unref);
#endif
    if (error)
        handle_dbus_call_error("org.gnome.SessionManager.Inhibit", &error);
}

static void gnome_integration_uninhibit_automount(SpiceDesktopIntegration *self)
{
    SpiceDesktopIntegrationPrivate *priv = self->priv;
    GError *error = NULL;

    if (!priv->gnome_session_proxy)
        return;

    /* Cookie is 0 when we failed to inhibit (and when called from dispose) */
    if (priv->gnome_automount_inhibit_cookie == 0)
        return;

#if defined(USE_GDBUS)
    GVariant *v = g_dbus_proxy_call_sync(priv->gnome_session_proxy,
                "Uninhibit",
                g_variant_new("(u)",
                              priv->gnome_automount_inhibit_cookie),
                G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    g_clear_pointer(&v, g_variant_unref);
#endif
    if (error)
        handle_dbus_call_error("org.gnome.SessionManager.Uninhibit", &error);

    priv->gnome_automount_inhibit_cookie = 0;
}

static void gnome_integration_dispose(SpiceDesktopIntegration *self)
{
    SpiceDesktopIntegrationPrivate *priv = self->priv;

    g_clear_object(&priv->gnome_session_proxy);
}

/* ------------------------------------------------------------------ */
/* gobject glue                                                       */

static void spice_desktop_integration_init(SpiceDesktopIntegration *self)
{
    SpiceDesktopIntegrationPrivate *priv;

    priv = SPICE_DESKTOP_INTEGRATION_GET_PRIVATE(self);
    self->priv = priv;

    if (!gnome_integration_init(self))
       g_warning("Warning no automount-inhibiting implementation available");
}

static void spice_desktop_integration_dispose(GObject *gobject)
{
    SpiceDesktopIntegration *self = SPICE_DESKTOP_INTEGRATION(gobject);

    gnome_integration_dispose(self);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_desktop_integration_parent_class)->dispose)
        G_OBJECT_CLASS(spice_desktop_integration_parent_class)->dispose(gobject);
}

static void spice_desktop_integration_class_init(SpiceDesktopIntegrationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose      = spice_desktop_integration_dispose;

    g_type_class_add_private(klass, sizeof(SpiceDesktopIntegrationPrivate));
}

SpiceDesktopIntegration *spice_desktop_integration_get(SpiceSession *session)
{
    SpiceDesktopIntegration *self;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_val_if_fail(session != NULL, NULL);

    g_static_mutex_lock(&mutex);
    self = g_object_get_data(G_OBJECT(session), "spice-desktop");
    if (self == NULL) {
        self = g_object_new(SPICE_TYPE_DESKTOP_INTEGRATION, NULL);
        g_object_set_data_full(G_OBJECT(session), "spice-desktop", self, g_object_unref);
    }
    g_static_mutex_unlock(&mutex);

    return self;
}

void spice_desktop_integration_inhibit_automount(SpiceDesktopIntegration *self)
{
    gnome_integration_inhibit_automount(self);
}

void spice_desktop_integration_uninhibit_automount(SpiceDesktopIntegration *self)
{
    gnome_integration_uninhibit_automount(self);
}
