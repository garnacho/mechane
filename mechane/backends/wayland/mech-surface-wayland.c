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

#include "mech-surface-wayland.h"
#include "mech-surface-wayland-shm.h"
#include "mech-backend-wayland.h"

G_DEFINE_ABSTRACT_TYPE (MechSurfaceWayland, mech_surface_wayland,
                        MECH_TYPE_SURFACE)

enum {
  PROP_WL_SURFACE = 1
};

struct _MechSurfaceWaylandPriv
{
  struct wl_surface *wl_surface;
};

static void
mech_surface_wayland_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MechSurfaceWaylandPriv *priv = ((MechSurfaceWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_SURFACE:
      priv->wl_surface = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_surface_wayland_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MechSurfaceWaylandPriv *priv = ((MechSurfaceWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_SURFACE:
      g_value_set_pointer (value, priv->wl_surface);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
mech_surface_wayland_class_init (MechSurfaceWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_surface_wayland_set_property;
  object_class->get_property = mech_surface_wayland_get_property;

  g_object_class_install_property (object_class,
                                   PROP_WL_SURFACE,
                                   g_param_spec_pointer ("wl-surface",
                                                         "wl-surface",
                                                         "wl-surface",
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (MechSurfaceWaylandPriv));
}

static void
mech_surface_wayland_init (MechSurfaceWayland *surface)
{
  surface->_priv = G_TYPE_INSTANCE_GET_PRIVATE (surface,
                                                MECH_TYPE_SURFACE_WAYLAND,
                                                MechSurfaceWaylandPriv);
}

MechSurface *
_mech_surface_wayland_new (MechBackingSurfaceType  type,
                           struct wl_surface      *wl_surface)
{
  return g_object_new (MECH_BACKING_SURFACE_TYPE_SHM,
                       "wl-surface", wl_surface,
                       NULL);
}

gboolean
_mech_surface_wayland_attach (MechSurfaceWayland *surface)
{
  MechSurfaceWaylandClass *surface_class;

  g_return_val_if_fail (MECH_IS_SURFACE_WAYLAND (surface), FALSE);

  surface_class = MECH_SURFACE_WAYLAND_GET_CLASS (surface);

  if (!surface_class->attach)
    return TRUE;

  return surface_class->attach (surface);
}

void
_mech_surface_wayland_damage (MechSurfaceWayland   *surface,
                              const cairo_region_t *region)
{
  g_return_if_fail (MECH_IS_SURFACE_WAYLAND (surface));
  g_return_if_fail (region != NULL);

  MECH_SURFACE_WAYLAND_GET_CLASS (surface)->damage (surface, region);
}

void
_mech_surface_wayland_translate (MechSurfaceWayland     *surface,
                                 gint                    tx,
                                 gint                    ty)
{
  g_return_if_fail (MECH_IS_SURFACE_WAYLAND (surface));

  MECH_SURFACE_WAYLAND_GET_CLASS (surface)->translate (surface, tx, ty);
}
