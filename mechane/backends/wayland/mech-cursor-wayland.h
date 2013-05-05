/* Mechane:
 * Copyright (C) 2013 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __MECH_CURSOR_WAYLAND_H__
#define __MECH_CURSOR_WAYLAND_H__

#include <mechane/mech-cursor.h>
#include <wayland-client.h>

G_BEGIN_DECLS

#define MECH_TYPE_CURSOR_WAYLAND         (mech_cursor_wayland_get_type ())
#define MECH_CURSOR_WAYLAND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_CURSOR_WAYLAND, MechCursorWayland))
#define MECH_CURSOR_WAYLAND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_CURSOR_WAYLAND, MechCursorWaylandClass))
#define MECH_IS_CURSOR_WAYLAND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_CURSOR_WAYLAND))
#define MECH_IS_CURSOR_WAYLAND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_CURSOR_WAYLAND))
#define MECH_CURSOR_WAYLAND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_CURSOR_WAYLAND, MechCursorWaylandClass))

typedef struct _MechCursorWayland MechCursorWayland;
typedef struct _MechCursorWaylandClass MechCursorWaylandClass;
typedef struct _MechCursorWaylandPriv MechCursorWaylandPriv;

struct _MechCursorWayland
{
  MechCursor parent_instance;
  MechCursorWaylandPriv *_priv;
};

struct _MechCursorWaylandClass
{
  MechCursorClass parent_class;
};

GType              mech_cursor_wayland_get_type   (void) G_GNUC_CONST;

struct wl_buffer * mech_cursor_wayland_get_buffer (MechCursorWayland *cursor,
                                                   gint              *width,
                                                   gint              *height,
                                                   gint              *hotspot_x,
                                                   gint              *hotspot_y);

G_END_DECLS

#endif /* __MECH_CURSOR_WAYLAND_H__ */
