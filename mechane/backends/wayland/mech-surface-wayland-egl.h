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

#ifndef __MECH_SURFACE_WAYLAND_EGL_H__
#define __MECH_SURFACE_WAYLAND_EGL_H__

#include "mech-surface-wayland.h"

G_BEGIN_DECLS

#define MECH_TYPE_SURFACE_WAYLAND_EGL         (mech_surface_wayland_egl_get_type ())
#define MECH_SURFACE_WAYLAND_EGL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SURFACE_WAYLAND_EGL, MechSurfaceWaylandEGL))
#define MECH_SURFACE_WAYLAND_EGL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_SURFACE_WAYLAND_EGL, MechSurfaceWaylandEGLClass))
#define MECH_IS_SURFACE_WAYLAND_EGL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SURFACE_WAYLAND_EGL))
#define MECH_IS_SURFACE_WAYLAND_EGL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_SURFACE_WAYLAND_EGL))
#define MECH_SURFACE_WAYLAND_EGL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_SURFACE_WAYLAND_EGL, MechSurfaceWaylandEGLClass))

typedef struct _MechSurfaceWaylandEGL MechSurfaceWaylandEGL;
typedef struct _MechSurfaceWaylandEGLClass MechSurfaceWaylandEGLClass;
typedef struct _MechSurfaceWaylandEGLPriv MechSurfaceWaylandEGLPriv;

struct _MechSurfaceWaylandEGL
{
  MechSurfaceWayland parent_instance;
  MechSurfaceWaylandEGLPriv *_priv;
};

struct _MechSurfaceWaylandEGLClass
{
  MechSurfaceWaylandClass parent_class;
};

GType mech_surface_wayland_egl_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __MECH_SURFACE_WAYLAND_EGL_H__ */
