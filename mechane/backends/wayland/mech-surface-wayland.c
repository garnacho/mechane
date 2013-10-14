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
#include "mech-surface-wayland-texture.h"
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
mech_surface_wayland_constructed (GObject *object)
{
  MechSurfaceWaylandPriv *priv = ((MechSurfaceWayland *) object)->_priv;
  MechSurfaceType surface_type;

  G_OBJECT_CLASS (mech_surface_wayland_parent_class)->constructed (object);

  surface_type = _mech_surface_get_surface_type ((MechSurface *) object);

  if (surface_type != MECH_SURFACE_TYPE_OFFSCREEN)
    {
      MechBackendWayland *backend;

      backend = _mech_backend_wayland_get ();
      priv->wl_surface = wl_compositor_create_surface (backend->wl_compositor);
    }
}

static void
mech_surface_wayland_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MechSurfaceWaylandPriv *priv = ((MechSurfaceWayland *) object)->_priv;

  switch (prop_id)
    {
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
mech_surface_wayland_finalize (GObject *object)
{
  MechSurfaceWaylandPriv *priv;

  priv = ((MechSurfaceWayland *) object)->_priv;

  if (priv->wl_surface)
    wl_surface_destroy (priv->wl_surface);

  G_OBJECT_CLASS (mech_surface_wayland_parent_class)->finalize (object);
}

static void
mech_surface_wayland_push_update (MechSurface          *surface,
                                  const cairo_region_t *region)
{
  MechSurfaceWaylandPriv *priv;

  priv = ((MechSurfaceWayland *) surface)->_priv;

  if (priv->wl_surface)
    wl_surface_commit (priv->wl_surface);
}

static void
mech_surface_wayland_class_init (MechSurfaceWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MechSurfaceClass *surface_class = MECH_SURFACE_CLASS (klass);

  object_class->constructed = mech_surface_wayland_constructed;
  object_class->set_property = mech_surface_wayland_set_property;
  object_class->get_property = mech_surface_wayland_get_property;
  object_class->finalize = mech_surface_wayland_finalize;

  surface_class->push_update = mech_surface_wayland_push_update;

  g_object_class_install_property (object_class,
                                   PROP_WL_SURFACE,
                                   g_param_spec_pointer ("wl-surface",
                                                         "wl-surface",
                                                         "wl-surface",
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

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
_mech_surface_wayland_new (MechSurfaceType  surface_type,
                           MechSurface     *parent)
{
  GType gtype = G_TYPE_NONE;

  g_assert (surface_type != MECH_SURFACE_TYPE_NONE);

  if (surface_type == MECH_SURFACE_TYPE_GL)
    gtype = MECH_TYPE_SURFACE_WAYLAND_EGL;
  else if (surface_type == MECH_SURFACE_TYPE_SOFTWARE)
    gtype = MECH_TYPE_SURFACE_WAYLAND_SHM;
  else if (surface_type == MECH_SURFACE_TYPE_OFFSCREEN)
    {
      g_assert (parent != NULL);

      /* Make an offscreen suitable to the parent renderer */
      if (MECH_IS_SURFACE_WAYLAND_SHM (parent))
        gtype = MECH_TYPE_SURFACE_WAYLAND_SHM;
      else if (MECH_IS_SURFACE_WAYLAND_EGL (parent) ||
               MECH_IS_SURFACE_WAYLAND_TEXTURE (parent))
        gtype = MECH_TYPE_SURFACE_WAYLAND_TEXTURE;
      else
        g_assert_not_reached ();
    }
  else
    g_assert_not_reached ();

  return g_object_new (gtype,
                       "surface-type", surface_type,
                       "parent", parent,
                       NULL);
}

void
_mech_surface_wayland_translate (MechSurfaceWayland     *surface,
                                 gint                    tx,
                                 gint                    ty)
{
  g_return_if_fail (MECH_IS_SURFACE_WAYLAND (surface));

  MECH_SURFACE_WAYLAND_GET_CLASS (surface)->translate (surface, tx, ty);
}
