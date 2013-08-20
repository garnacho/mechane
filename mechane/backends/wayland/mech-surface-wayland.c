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
#include "mech-surface-wayland-egl.h"
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
  GType gtype = G_TYPE_NONE;
  MechSurface *surface;
  GError *error = NULL;

  g_return_val_if_fail (type == MECH_BACKING_SURFACE_TYPE_EGL ||
                        type == MECH_BACKING_SURFACE_TYPE_SHM, NULL);

  if (type == MECH_BACKING_SURFACE_TYPE_SHM)
    gtype = MECH_TYPE_SURFACE_WAYLAND_SHM;
  else if (type == MECH_BACKING_SURFACE_TYPE_EGL)
    gtype = MECH_TYPE_SURFACE_WAYLAND_EGL;
  else
    return NULL;

  surface = g_object_new (gtype,
                          "wl-surface", wl_surface,
                          NULL);

  if (!G_IS_INITABLE (surface))
    return surface;

  if (!g_initable_init (G_INITABLE (surface), NULL, &error))
    {
      if (gtype == MECH_TYPE_SURFACE_WAYLAND_EGL)
        {
          g_warning ("Could not create EGL surface, falling "
                     "back to SHM ones. The error was: %s",
                     error->message);
          g_error_free (error);

          return _mech_surface_wayland_new (MECH_BACKING_SURFACE_TYPE_SHM,
                                            wl_surface);
        }

      g_critical ("Could not a create a surface "
                  "of type '%s', The error was: %s",
                  g_type_name (gtype), error->message);
      g_error_free (error);
    }

  return surface;
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
