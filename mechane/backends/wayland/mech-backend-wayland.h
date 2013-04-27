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

#ifndef __MECH_BACKEND_WAYLAND_H__
#define __MECH_BACKEND_WAYLAND_H__

#include <wayland-client.h>
#include <mechane/mech-backend-private.h>

G_BEGIN_DECLS

#define MECH_TYPE_BACKEND_WAYLAND         (_mech_backend_wayland_get_type ())
#define MECH_BACKEND_WAYLAND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_BACKEND_WAYLAND, MechBackendWayland))
#define MECH_BACKEND_WAYLAND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_BACKEND_WAYLAND, MechBackendWaylandClass))
#define MECH_IS_BACKEND_WAYLAND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_BACKEND_WAYLAND))
#define MECH_IS_BACKEND_WAYLAND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_BACKEND_WAYLAND))
#define MECH_BACKEND_WAYLAND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_BACKEND_WAYLAND, MechBackendWaylandClass))

typedef struct _MechBackendWayland MechBackendWayland;
typedef struct _MechBackendWaylandClass MechBackendWaylandClass;
typedef struct _MechBackendWaylandPriv MechBackendWaylandPriv;

struct _MechBackendWayland
{
  MechBackend parent_instance;
  MechBackendWaylandPriv *_priv;
  struct wl_display *wl_display;
  struct wl_registry *wl_registry;
  struct wl_compositor *wl_compositor;
  struct wl_shm *wl_shm;
  struct wl_shell *wl_shell;
};

struct _MechBackendWaylandClass
{
  MechBackendClass parent_class;
};

GType                _mech_backend_wayland_get_type      (void) G_GNUC_CONST;

MechBackendWayland * _mech_backend_wayland_get           (void);

G_END_DECLS

#endif /* __MECH_BACKEND_WAYLAND_H__ */
