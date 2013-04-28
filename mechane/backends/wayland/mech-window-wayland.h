/* Mechane:
 * Copyright (C) 2012 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MECH_WINDOW_WAYLAND_H__
#define __MECH_WINDOW_WAYLAND_H__

#include <wayland-client.h>
#include <mechane/mech-window.h>

G_BEGIN_DECLS

#define MECH_TYPE_WINDOW_WAYLAND         (mech_window_wayland_get_type ())
#define MECH_WINDOW_WAYLAND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_WINDOW_WAYLAND, MechWindowWayland))
#define MECH_WINDOW_WAYLAND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_WINDOW_WAYLAND, MechWindowWaylandClass))
#define MECH_IS_WINDOW_WAYLAND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_WINDOW_WAYLAND))
#define MECH_IS_WINDOW_WAYLAND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_WINDOW_WAYLAND))
#define MECH_WINDOW_WAYLAND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_WINDOW_WAYLAND, MechWindowWaylandClass))

typedef struct _MechWindowWayland MechWindowWayland;
typedef struct _MechWindowWaylandClass MechWindowWaylandClass;
typedef struct _MechWindowWaylandPriv MechWindowWaylandPriv;

struct _MechWindowWayland
{
  MechWindow parent_instance;
  MechWindowWaylandPriv *_priv;
};

struct _MechWindowWaylandClass
{
  MechWindowClass parent_class;
};

GType             mech_window_wayland_get_type    (void) G_GNUC_CONST;

MechWindow *     _mech_window_wayland_new         (struct wl_compositor *compositor);

G_END_DECLS

#endif /* __MECH_WINDOW_WAYLAND_H__ */
