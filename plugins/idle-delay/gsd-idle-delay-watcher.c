/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Linux Deepin Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "gsd-idle-delay-misc.h"
#include "gsd-idle-delay-marshal.h"
#include "gsd-idle-delay-watcher.h"

static void     gsd_idle_delay_watcher_class_init (GsdIdleDelayWatcherClass *klass);
static void     gsd_idle_delay_watcher_init       (GsdIdleDelayWatcher      *watcher);
static void     gsd_idle_delay_watcher_finalize   (GObject        	    *object);

static gboolean watchdog_timer (GsdIdleDelayWatcher *watcher);
#define GSD_IDLE_DELAY_WATCHER_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_IDLE_DELAY_WATCHER, GsdIdleDelayWatcherPrivate))

struct GsdIdleDelayWatcherPrivate
{
        /* settingsd_idle_delay */
        guint           enabled : 1;  //enable or disable Idle detection

        /* state */
        guint           active : 1;
        guint           idle : 1;
        guint           idle_notice : 1;

        GDBusProxy	*presence_proxy; //gnome session presence dbus
        guint           watchdog_timer_id; //disable X server builtin screensaver
};

enum {
        PROP_0,
};

enum {
        IDLE_CHANGED,
        IDLE_NOTICE_CHANGED,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GsdIdleDelayWatcher, gsd_idle_delay_watcher, G_TYPE_OBJECT)


static void
remove_watchdog_timer (GsdIdleDelayWatcher *watcher)
{
        if (watcher->priv->watchdog_timer_id != 0) {
                g_source_remove (watcher->priv->watchdog_timer_id);
                watcher->priv->watchdog_timer_id = 0;
        }
}

static void
add_watchdog_timer (GsdIdleDelayWatcher *watcher,
                    glong      timeout)
{
        watcher->priv->watchdog_timer_id = g_timeout_add (timeout,
                                                          (GSourceFunc)watchdog_timer,
                                                          watcher);
}

static void
gsd_idle_delay_watcher_get_property (GObject    *object,
                         	     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_idle_delay_watcher_set_property (GObject          *object,
                         	     guint             prop_id,
                         	     const GValue     *value,
                         	     GParamSpec       *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_idle_delay_watcher_class_init (GsdIdleDelayWatcherClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gsd_idle_delay_watcher_finalize;
        object_class->get_property = gsd_idle_delay_watcher_get_property;
        object_class->set_property = gsd_idle_delay_watcher_set_property;

        signals [IDLE_CHANGED] =
                g_signal_new ("idle-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GsdIdleDelayWatcherClass, idle_changed),
                              NULL,
                              NULL,
                              gsd_idle_delay_marshal_BOOLEAN__BOOLEAN,
                              G_TYPE_BOOLEAN,
                              1, G_TYPE_BOOLEAN);
        signals [IDLE_NOTICE_CHANGED] =
                g_signal_new ("idle-notice-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GsdIdleDelayWatcherClass, idle_notice_changed),
                              NULL,
                              NULL,
                              gsd_idle_delay_marshal_BOOLEAN__BOOLEAN,
                              G_TYPE_BOOLEAN,
                              1, G_TYPE_BOOLEAN);

        g_type_class_add_private (klass, sizeof (GsdIdleDelayWatcherPrivate));
}

static gboolean
_gsd_idle_delay_watcher_set_session_idle_notice (GsdIdleDelayWatcher *watcher,
                                     		 gboolean   in_effect)
{
        gboolean res = FALSE;

        if (in_effect != watcher->priv->idle_notice) 
	{
                g_signal_emit (watcher, signals [IDLE_NOTICE_CHANGED], 0, in_effect, &res);
                if (res) 
		{
                        g_debug ("Changing idle notice state: %d", in_effect);
                        watcher->priv->idle_notice = in_effect;
                } 
		else 
		{
                        g_debug ("Idle notice signal not handled: %d", in_effect);
                }
        }

        return res;
}

static gboolean
_gsd_idle_delay_watcher_set_session_idle (GsdIdleDelayWatcher *watcher,
                              		  gboolean   is_idle)
{
        gboolean res = FALSE;

        if (is_idle != watcher->priv->idle) 
	{
                g_signal_emit (watcher, signals [IDLE_CHANGED], 0, is_idle, &res);
                if (res) 
		{
                        g_debug ("Changing idle state: %d", is_idle);
                        watcher->priv->idle = is_idle;
                } 
		else 
		{
                        g_debug ("Idle changed signal not handled: %d", is_idle);
                }
        }

        return res;
}

gboolean
gsd_idle_delay_watcher_get_active (GsdIdleDelayWatcher *watcher)
{
        g_return_val_if_fail (GSD_IS_IDLE_DELAY_WATCHER (watcher), FALSE);
        return watcher->priv->active;
}

static void
_gsd_idle_delay_watcher_reset_state (GsdIdleDelayWatcher *watcher)
{
        watcher->priv->idle = FALSE;
        watcher->priv->idle_notice = FALSE;
}

static gboolean
_gsd_idle_delay_watcher_set_active_internal (GsdIdleDelayWatcher *watcher,
                                 	     gboolean   active)
{
        if (active != watcher->priv->active) 
	{
                /* reset state */
                _gsd_idle_delay_watcher_reset_state (watcher);
                watcher->priv->active = active;
        }

        return TRUE;
}

gboolean
gsd_idle_delay_watcher_set_active (GsdIdleDelayWatcher *watcher,
                       		   gboolean   active)
{
        g_return_val_if_fail (GSD_IS_IDLE_DELAY_WATCHER (watcher), FALSE);
        g_debug ("turning watcher: %s", active ? "ON" : "OFF");

        if (watcher->priv->active == active) 
	{
                g_debug ("Idle detection is already %s", active ? "active" : "inactive");
                return FALSE;
        }
        if (! watcher->priv->enabled) 
	{
                g_debug ("Idle detection is disabled, cannot activate");
                return FALSE;
        }

        return _gsd_idle_delay_watcher_set_active_internal (watcher, active);
}

gboolean
gsd_idle_delay_watcher_set_enabled (GsdIdleDelayWatcher *watcher,
                        gboolean   enabled)
{
        g_return_val_if_fail (GSD_IS_IDLE_DELAY_WATCHER (watcher), FALSE);

        if (watcher->priv->enabled != enabled) 
	{
                gboolean is_active = gsd_idle_delay_watcher_get_active (watcher);

                watcher->priv->enabled = enabled;

                /* if we are disabling the watcher and we are
                   active shut it down */
                if (! enabled && is_active) 
		{
                        _gsd_idle_delay_watcher_set_active_internal (watcher, FALSE);
                }
        }

        return TRUE;
}

gboolean
gsd_idle_delay_watcher_get_enabled (GsdIdleDelayWatcher *watcher)
{
        g_return_val_if_fail (GSD_IS_IDLE_DELAY_WATCHER (watcher), FALSE);

        return watcher->priv->enabled;
}

static void
set_status (GsdIdleDelayWatcher *watcher,
            guint      status)
{
	g_debug ("set status: %d", status);
        gboolean is_idle;

        if (! watcher->priv->active) 
	{
                g_debug ("GsdIdleDelayWatcher: not active, ignoring status changes");
                return;
        }

        is_idle = (status == 3);

        if (!is_idle && !watcher->priv->idle_notice) 
	{
                /* no change in idleness */
                return;
        }

        if (is_idle) 
	{
                _gsd_idle_delay_watcher_set_session_idle_notice (watcher, is_idle);
        }
	else 
	{
                _gsd_idle_delay_watcher_set_session_idle (watcher, FALSE);
                _gsd_idle_delay_watcher_set_session_idle_notice (watcher, FALSE);
        }
}

static void
on_presence_status_changed (GDBusProxy		*presence_proxy,
                            gchar		*send_name,
			    gchar		*signal_name,
			    GVariant		*parameters,
                            GsdIdleDelayWatcher *watcher)
{
	g_debug ("on presence status changed");

	guint status;
	g_variant_get (parameters, "(u)", &status);

        set_status (watcher, status);
}

static void
connect_presence_watcher (GsdIdleDelayWatcher *watcher)
{
	g_debug ("connect presence watcher");

        GError *error;

	//1. connect to gnome-session presence.
        error = NULL;
        watcher->priv->presence_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
								       G_DBUS_PROXY_FLAGS_NONE,
								       NULL,
								       GSM_SERVICE,
								       GSM_PRESENCE_PATH,
								       GSM_PRESENCE_INTERFACE,
								       NULL,
                                                                       &error);
        if (error != NULL) 
	{
                g_warning ("Couldn't connect to session presence : %s", error->message);
                g_error_free (error);
                return;
	}
	g_signal_connect (watcher->priv->presence_proxy,
			  "g-signal",
			  G_CALLBACK (on_presence_status_changed),
			  watcher);
	//2. get initial status value.
	GDBusProxy* _proxy;
	GVariant*   _result;
        error = NULL;
        _proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
						G_DBUS_PROXY_FLAGS_NONE,
						NULL,
						GSM_SERVICE,
						GSM_PRESENCE_PATH,
						DBUS_PROP_INTERFACE,
						NULL,
                                                &error);
	if (error != NULL) 
	{
                g_warning ("Couldn't connect to session properties: %s", error->message);
                g_error_free (error);
                return;
	}
        error = NULL;
	_result = g_dbus_proxy_call_sync (_proxy, 
					  "Get",
					  g_variant_new ("(ss)", GSM_PRESENCE_INTERFACE, "status"),
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  &error);
	g_object_unref (_proxy);
	if (error != NULL) 
	{
                g_warning ("Couldn't get session properties: %s", error->message);
                g_error_free (error);
                return;
	}

	GVariant* _tmp;
	g_variant_get (_result, "(v)", &_tmp);

        guint status = g_variant_get_uint32 (_tmp);
	g_variant_unref (_tmp);
	g_variant_unref (_result);
        set_status (watcher, status);
}

static void
gsd_idle_delay_watcher_init (GsdIdleDelayWatcher *watcher)
{
        watcher->priv = GSD_IDLE_DELAY_WATCHER_GET_PRIVATE (watcher);

        watcher->priv->enabled = TRUE;
        watcher->priv->active = FALSE;

        connect_presence_watcher (watcher);

        add_watchdog_timer (watcher, 600000);
}

static void
gsd_idle_delay_watcher_finalize (GObject *object)
{
        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_IDLE_DELAY_WATCHER (object));

        GsdIdleDelayWatcher *watcher = GSD_IDLE_DELAY_WATCHER (object);

        g_return_if_fail (watcher->priv != NULL);

        remove_watchdog_timer (watcher);

        watcher->priv->active = FALSE;

        if (watcher->priv->presence_proxy != NULL) 
                g_object_unref (watcher->priv->presence_proxy);

        G_OBJECT_CLASS (gsd_idle_delay_watcher_parent_class)->finalize (object);
}

// copied from gnome-screensaver
/* Figuring out what the appropriate XSetScreenSaver() parameters are
   (one wouldn't expect this to be rocket science.)
*/
static void
disable_builtin_screensaver (GsdIdleDelayWatcher *watcher,
                             gboolean   unblank_screen)
{
        int current_server_timeout, current_server_interval;
        int current_prefer_blank,   current_allow_exp;
        int desired_server_timeout, desired_server_interval;
        int desired_prefer_blank,   desired_allow_exp;

        XGetScreenSaver (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                         &current_server_timeout,
                         &current_server_interval,
                         &current_prefer_blank,
                         &current_allow_exp);

        desired_server_timeout  = current_server_timeout;
        desired_server_interval = current_server_interval;
        desired_prefer_blank    = current_prefer_blank;
        desired_allow_exp       = current_allow_exp;

        desired_server_interval = 0;

        /* I suspect (but am not sure) that DontAllowExposures might have
           something to do with powering off the monitor as well, at least
           on some systems that don't support XDPMS?  Who know... */
        desired_allow_exp = AllowExposures;

        /* When we're not using an extension, set the server-side timeout to 0,
           so that the server never gets involved with screen blanking, and we
           do it all ourselves.  (However, when we *are* using an extension,
           we tell the server when to notify us, and rather than blanking the
           screen, the server will send us an X event telling us to blank.)
        */
        desired_server_timeout = 0;

        if (desired_server_timeout     != current_server_timeout
            || desired_server_interval != current_server_interval
            || desired_prefer_blank    != current_prefer_blank
            || desired_allow_exp       != current_allow_exp) {

                g_debug ("disabling server builtin screensaver:"
                          " (xset s %d %d; xset s %s; xset s %s)",
                          desired_server_timeout,
                          desired_server_interval,
                          (desired_prefer_blank ? "blank" : "noblank"),
                          (desired_allow_exp ? "expose" : "noexpose"));

                XSetScreenSaver (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                 desired_server_timeout,
                                 desired_server_interval,
                                 desired_prefer_blank,
                                 desired_allow_exp);

                XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), FALSE);
        }

        if (unblank_screen) {
                /* Turn off the server builtin saver if it is now running. */
                XForceScreenSaver (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), ScreenSaverReset);
        }
}


/* This timer goes off every few minutes, whether the user is idle or not,
   to try and clean up anything that has gone wrong.

   It calls disable_builtin_screensaver() so that if xset has been used,
   or some other program (like xlock) has messed with the XSetScreenSaver()
   settings, they will be set back to sensible values (if a server extension
   is in use, messing with xlock can cause the screensaver to never get a wakeup
   event, and could cause monitor power-saving to occur, and all manner of
   heinousness.)

 */

static gboolean
watchdog_timer (GsdIdleDelayWatcher *watcher)
{

        disable_builtin_screensaver (watcher, FALSE);

        return TRUE;
}

GsdIdleDelayWatcher *
gsd_idle_delay_watcher_new (void)
{
        GsdIdleDelayWatcher *watcher;
        watcher = g_object_new (GSD_TYPE_IDLE_DELAY_WATCHER, NULL);

        return GSD_IDLE_DELAY_WATCHER (watcher);
}
