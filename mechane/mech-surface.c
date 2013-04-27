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

#include <math.h>
#include "mech-surface-private.h"
#include "mech-area-private.h"

typedef struct _MechSurfacePrivate MechSurfacePrivate;

enum {
  PROP_AREA = 1,
};

struct _MechSurfacePrivate
{
  MechArea *area;

  /* Pixel sizes */
  gint width;
  gint height;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechSurface, mech_surface, G_TYPE_OBJECT)

static void
mech_surface_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private ((MechSurface *) object);

  switch (prop_id)
    {
    case PROP_AREA:
      g_value_set_object (value, priv->area);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_surface_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private ((MechSurface *) object);

  switch (prop_id)
    {
    case PROP_AREA:
      priv->area = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_surface_class_init (MechSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mech_surface_get_property;
  object_class->set_property = mech_surface_set_property;

  g_object_class_install_property (object_class,
                                   PROP_AREA,
                                   g_param_spec_object ("area",
                                                        "area",
                                                        "area",
                                                        MECH_TYPE_AREA,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
mech_surface_init (MechSurface *surface)
{
}

void
_mech_surface_set_size (MechSurface *surface,
                        gint         width,
                        gint         height)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);
  priv->width = width;
  priv->height = height;
}

void
_mech_surface_get_size (MechSurface *surface,
                        gint        *width,
                        gint        *height)
{
  MechSurfacePrivate *priv;

  priv = mech_surface_get_instance_private (surface);

  if (width)
    *width = priv->width;
  if (height)
    *height = priv->height;
}

cairo_t *
_mech_surface_cairo_create (MechSurface *surface)
{
  cairo_surface_t *cairo_surface;
  cairo_t *cr;

  cairo_surface = MECH_SURFACE_GET_CLASS (surface)->get_surface (surface);
  g_assert (cairo_surface != NULL);

  cr = cairo_create (cairo_surface);

  return cr;
}
