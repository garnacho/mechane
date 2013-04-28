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

#ifndef __MECH_CLOCK_WAYLAND_H__
#define __MECH_CLOCK_WAYLAND_H__

#include <wayland-client.h>
#include <mechane/mech-clock-private.h>
#include "mech-window-wayland.h"

G_BEGIN_DECLS

#define MECH_TYPE_CLOCK_WAYLAND         (mech_clock_wayland_get_type ())
#define MECH_CLOCK_WAYLAND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_CLOCK_WAYLAND, MechClockWayland))
#define MECH_CLOCK_WAYLAND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_CLOCK_WAYLAND, MechClockWaylandClass))
#define MECH_IS_CLOCK_WAYLAND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_CLOCK_WAYLAND))
#define MECH_IS_CLOCK_WAYLAND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_CLOCK_WAYLAND))
#define MECH_CLOCK_WAYLAND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_CLOCK_WAYLAND, MechClockWaylandClass))

typedef struct _MechClockWayland MechClockWayland;
typedef struct _MechClockWaylandClass MechClockWaylandClass;
typedef struct _MechClockWaylandPriv MechClockWaylandPriv;

struct _MechClockWayland
{
  MechClock parent_instance;
  MechClockWaylandPriv *_priv;
};

struct _MechClockWaylandClass
{
  MechClockClass parent_class;
};

GType       mech_clock_wayland_get_type (void) G_GNUC_CONST;

MechClock * _mech_clock_wayland_new     (MechWindowWayland *window,
                                         struct wl_surface *surface);

G_END_DECLS

#endif /* __MECH_CLOCK_WAYLAND_H__ */
