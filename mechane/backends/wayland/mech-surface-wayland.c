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
#include "subsurface-client-protocol.h"

G_DEFINE_ABSTRACT_TYPE (MechSurfaceWayland, mech_surface_wayland,
                        MECH_TYPE_SURFACE)

enum {
  PROP_WL_SURFACE = 1
};

struct _MechSurfaceWaylandPriv
{
  struct wl_surface *wl_surface;
  struct wl_subsurface *wl_subsurface;
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

  if (priv->wl_subsurface)
    wl_subsurface_destroy (priv->wl_subsurface);

  if (priv->wl_surface)
    wl_surface_destroy (priv->wl_surface);

  G_OBJECT_CLASS (mech_surface_wayland_parent_class)->finalize (object);
}

static void
_set_empty_input_region (MechBackendWayland *backend,
                         struct wl_surface  *wl_surface)
{
  struct wl_region *wl_region;

  wl_region = wl_compositor_create_region (backend->wl_compositor);
  wl_surface_set_input_region (wl_surface, wl_region);
  wl_region_destroy (wl_region);
}

static gboolean
mech_surface_wayland_set_parent (MechSurface *surface,
                                 MechSurface *parent)
{
  MechSurfaceType surface_type, parent_type;
  MechSurfaceWaylandPriv *priv;

  surface_type = _mech_surface_get_surface_type (surface);
  priv = ((MechSurfaceWayland *) surface)->_priv;

  if (priv->wl_subsurface)
    {
      wl_subsurface_destroy (priv->wl_subsurface);
      priv->wl_subsurface = NULL;
    }

  if (parent && surface_type != MECH_SURFACE_TYPE_OFFSCREEN)
    {
      struct wl_surface *parent_wl_surface;
      MechBackendWayland *backend;

      backend = _mech_backend_wayland_get ();
      parent_type = _mech_surface_get_surface_type (parent);
      g_object_get (parent, "wl-surface", &parent_wl_surface, NULL);

      if (!backend->wl_subcompositor)
        return FALSE;

      if (parent_type == MECH_SURFACE_TYPE_OFFSCREEN)
        return FALSE;

      if (!priv->wl_surface || !parent_wl_surface)
        return FALSE;

      _set_empty_input_region (backend, priv->wl_surface);

      priv->wl_subsurface =
        wl_subcompositor_get_subsurface (backend->wl_subcompositor,
                                         priv->wl_surface,
                                         parent_wl_surface);
      return TRUE;
    }

  return MECH_SURFACE_CLASS (mech_surface_wayland_parent_class)->set_parent (surface, parent);
}

static void
mech_surface_wayland_set_above (MechSurface *surface,
                                MechSurface *sibling)
{
  MechSurfaceWaylandPriv *priv, *above_priv;
  MechSurface *above = NULL;

  priv = ((MechSurfaceWayland *) surface)->_priv;

  if (!priv->wl_subsurface)
    return;

  if (sibling)
    {
      above_priv = ((MechSurfaceWayland *) sibling)->_priv;

      if (above_priv->wl_surface)
        above = sibling;
    }

  if (!above)
    above = _mech_surface_get_parent (surface);

  if (!above)
    return;

  above_priv = ((MechSurfaceWayland *) above)->_priv;
  wl_subsurface_place_above (priv->wl_subsurface,
                             above_priv->wl_surface);
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
mech_surface_wayland_render (MechSurface *surface,
                             cairo_t     *cr)
{
  MechSurfaceWaylandPriv *priv;

  priv = ((MechSurfaceWayland *) surface)->_priv;

  if (!priv->wl_subsurface)
    MECH_SURFACE_CLASS (mech_surface_wayland_parent_class)->render (surface, cr);
  else
    {
      gdouble x, y;

      x = y = 0;
      cairo_user_to_device (cr, &x, &y);
      wl_subsurface_set_position (priv->wl_subsurface, (gint) x, (gint) y);
    }
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

  surface_class->set_parent = mech_surface_wayland_set_parent;
  surface_class->set_above = mech_surface_wayland_set_above;
  surface_class->push_update = mech_surface_wayland_push_update;
  surface_class->render = mech_surface_wayland_render;

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
