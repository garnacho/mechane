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

#ifndef __MECH_SEAT_WAYLAND_H__
#define __MECH_SEAT_WAYLAND_H__

#include <mechane/mech-seat.h>
#include "mech-backend-wayland.h"

G_BEGIN_DECLS

#define MECH_TYPE_SEAT_WAYLAND         (mech_seat_wayland_get_type ())
#define MECH_SEAT_WAYLAND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SEAT_WAYLAND, MechSeatWayland))
#define MECH_SEAT_WAYLAND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_SEAT_WAYLAND, MechSeatWaylandClass))
#define MECH_IS_SEAT_WAYLAND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SEAT_WAYLAND))
#define MECH_IS_SEAT_WAYLAND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_SEAT_WAYLAND))
#define MECH_SEAT_WAYLAND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_SEAT_WAYLAND, MechSeatWaylandClass))

typedef struct _MechSeatWayland MechSeatWayland;
typedef struct _MechSeatWaylandClass MechSeatWaylandClass;
typedef struct _MechSeatWaylandPriv MechSeatWaylandPriv;

struct _MechSeatWayland
{
  MechSeat parent_instance;
  MechSeatWaylandPriv *_priv;
};

struct _MechSeatWaylandClass
{
  MechSeatClass parent_class;
};

GType      mech_seat_wayland_get_type     (void) G_GNUC_CONST;

MechSeat * mech_seat_wayland_new          (struct wl_seat  *seat);

G_END_DECLS

#endif /* __MECH_SEAT_WAYLAND_H__ */
