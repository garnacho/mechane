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

#ifndef __MECH_SURFACE_WAYLAND_SHM_H__
#define __MECH_SURFACE_WAYLAND_SHM_H__

#include "mech-surface-wayland.h"

G_BEGIN_DECLS

#define MECH_TYPE_SURFACE_WAYLAND_SHM         (mech_surface_wayland_shm_get_type ())
#define MECH_SURFACE_WAYLAND_SHM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SURFACE_WAYLAND_SHM, MechSurfaceWaylandSHM))
#define MECH_SURFACE_WAYLAND_SHM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_SURFACE_WAYLAND_SHM, MechSurfaceWaylandSHMClass))
#define MECH_IS_SURFACE_WAYLAND_SHM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SURFACE_WAYLAND_SHM))
#define MECH_IS_SURFACE_WAYLAND_SHM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_SURFACE_WAYLAND_SHM))
#define MECH_SURFACE_WAYLAND_SHM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_SURFACE_WAYLAND_SHM, MechSurfaceWaylandSHMClass))

typedef struct _MechSurfaceWaylandSHM MechSurfaceWaylandSHM;
typedef struct _MechSurfaceWaylandSHMClass MechSurfaceWaylandSHMClass;
typedef struct _MechSurfaceWaylandSHMPriv MechSurfaceWaylandSHMPriv;

struct _MechSurfaceWaylandSHM
{
  MechSurfaceWayland parent_instance;
  MechSurfaceWaylandSHMPriv *_priv;
};

struct _MechSurfaceWaylandSHMClass
{
  MechSurfaceWaylandClass parent_class;
};

GType mech_surface_wayland_shm_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __MECH_SURFACE_WAYLAND_SHM_H__ */
