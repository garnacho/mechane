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

#ifndef __MECH_SURFACE_WAYLAND_TEXTURE_H__
#define __MECH_SURFACE_WAYLAND_TEXTURE_H__

#include "mech-surface-wayland.h"

G_BEGIN_DECLS

#define MECH_TYPE_SURFACE_WAYLAND_TEXTURE         (mech_surface_wayland_texture_get_type ())
#define MECH_SURFACE_WAYLAND_TEXTURE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SURFACE_WAYLAND_TEXTURE, MechSurfaceWaylandTexture))
#define MECH_SURFACE_WAYLAND_TEXTURE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_SURFACE_WAYLAND_TEXTURE, MechSurfaceWaylandTextureClass))
#define MECH_IS_SURFACE_WAYLAND_TEXTURE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SURFACE_WAYLAND_TEXTURE))
#define MECH_IS_SURFACE_WAYLAND_TEXTURE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_SURFACE_WAYLAND_TEXTURE))
#define MECH_SURFACE_WAYLAND_TEXTURE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_SURFACE_WAYLAND_TEXTURE, MechSurfaceWaylandTextureClass))

typedef struct _MechSurfaceWaylandTexture MechSurfaceWaylandTexture;
typedef struct _MechSurfaceWaylandTextureClass MechSurfaceWaylandTextureClass;

struct _MechSurfaceWaylandTexture
{
  MechSurfaceWayland parent_instance;
};

struct _MechSurfaceWaylandTextureClass
{
  MechSurfaceWaylandClass parent_class;
};

GType          mech_surface_wayland_texture_get_type      (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __MECH_SURFACE_WAYLAND_TEXTURE_H__ */
