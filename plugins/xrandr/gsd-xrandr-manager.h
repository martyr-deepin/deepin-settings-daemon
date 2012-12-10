/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef __GSD_XRANDR_MANAGER_H
#define __GSD_XRANDR_MANAGER_H

#include <glib-object.h>
#include <gio/gio.h>
#include <libupower-glib/upower.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr-config.h>
#include <libgnome-desktop/gnome-rr.h>
#include <libgnome-desktop/gnome-rr-labeler.h>

G_BEGIN_DECLS

#define GSD_TYPE_XRANDR_MANAGER         (gsd_xrandr_manager_get_type ())
#define GSD_XRANDR_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_XRANDR_MANAGER, GsdXrandrManager))
#define GSD_XRANDR_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_XRANDR_MANAGER, GsdXrandrManagerClass))
#define GSD_IS_XRANDR_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_XRANDR_MANAGER))
#define GSD_IS_XRANDR_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_XRANDR_MANAGER))
#define GSD_XRANDR_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_XRANDR_MANAGER, GsdXrandrManagerClass))
#define CONF_SCHEMA "org.gnome.settings-daemon.plugins.xrandr"

typedef struct GsdXrandrManagerPrivate
{
    GnomeRRScreen *rw_screen;
    gboolean running;
    
    UpClient *upower_client;
    gboolean laptop_lid_is_closed;
    
    GSettings       *settings;
    GDBusNodeInfo   *introspection_data;
    GDBusConnection *connection;
    GCancellable    *bus_cancellable;

    /* fn-F7 status */
    int             current_fn_f7_config;             /* -1 if no configs */
    GnomeRRConfig **fn_f7_configs;  /* NULL terminated, NULL if there are no configs */

    /* Last time at which we got a "screen got reconfigured" event; see on_randr_event() */
    guint32 last_config_timestamp;
} GsdXrandrManagerPrivate;

typedef struct
{
        GObject                     parent;
        GsdXrandrManagerPrivate *priv;
} GsdXrandrManager;

typedef struct
{
        GObjectClass   parent_class;
} GsdXrandrManagerClass;

GType                   gsd_xrandr_manager_get_type            (void);

GsdXrandrManager *       gsd_xrandr_manager_new                 (void);
gboolean                gsd_xrandr_manager_start               (GsdXrandrManager *manager,
                                                               GError         **error);
void                    gsd_xrandr_manager_stop                (GsdXrandrManager *manager);

G_END_DECLS

#endif /* __GSD_XRANDR_MANAGER_H */
