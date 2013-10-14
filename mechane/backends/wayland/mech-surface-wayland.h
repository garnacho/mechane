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

#ifndef __MECH_SURFACE_WAYLAND_H__
#define __MECH_SURFACE_WAYLAND_H__

#include <wayland-client.h>
#include <mechane/mech-surface-private.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_SURFACE_WAYLAND         (mech_surface_wayland_get_type ())
#define MECH_SURFACE_WAYLAND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SURFACE_WAYLAND, MechSurfaceWayland))
#define MECH_SURFACE_WAYLAND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_SURFACE_WAYLAND, MechSurfaceWaylandClass))
#define MECH_IS_SURFACE_WAYLAND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SURFACE_WAYLAND))
#define MECH_IS_SURFACE_WAYLAND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_SURFACE_WAYLAND))
#define MECH_SURFACE_WAYLAND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_SURFACE_WAYLAND, MechSurfaceWaylandClass))

typedef struct _MechSurfaceWayland MechSurfaceWayland;
typedef struct _MechSurfaceWaylandClass MechSurfaceWaylandClass;
typedef struct _MechSurfaceWaylandPriv MechSurfaceWaylandPriv;

struct _MechSurfaceWayland
{
  MechSurface parent_instance;
  MechSurfaceWaylandPriv *_priv;
};

struct _MechSurfaceWaylandClass
{
  MechSurfaceClass parent_class;

  void     (* translate) (MechSurfaceWayland   *surface,
                          gint                  tx,
                          gint                  ty);
};

GType         mech_surface_wayland_get_type   (void) G_GNUC_CONST;

MechSurface * _mech_surface_wayland_new       (MechSurfaceType         surface_type,
                                               MechSurface            *parent);
void          _mech_surface_wayland_translate (MechSurfaceWayland     *surface,
                                               gint                    tx,
                                               gint                    ty);

G_END_DECLS

#endif /* __MECH_SURFACE_WAYLAND_H__ */
