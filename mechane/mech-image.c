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

#include "mech-pattern-private.h"
#include "mech-image.h"

typedef struct _MechImagePrivate MechImagePrivate;

enum {
  PROP_FILE = 1
};

struct _MechImagePrivate
{
  GFile *file;
  MechPattern *pattern;
};

G_DEFINE_TYPE_WITH_PRIVATE (MechImage, mech_image, MECH_TYPE_AREA)

static void
mech_image_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_FILE:
      mech_image_set_file (MECH_IMAGE (object),
                           g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_image_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  MechImagePrivate *priv;

  priv = mech_image_get_instance_private ((MechImage *) object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, priv->file);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_image_finalize (GObject *object)
{
  MechImagePrivate *priv;

  priv = mech_image_get_instance_private ((MechImage *) object);

  if (priv->pattern)
    _mech_pattern_unref (priv->pattern);

  if (priv->file)
    g_object_unref (priv->file);

  G_OBJECT_CLASS (mech_image_parent_class)->finalize (object);
}

static gdouble
mech_image_get_extent (MechArea *area,
                       MechAxis  axis)
{
  MechUnit width_unit, height_unit;
  MechImagePrivate *priv;
  gdouble width, height;

  priv = mech_image_get_instance_private ((MechImage *) area);

  if (!priv->pattern)
    return 0;

  /* FIXME: units */
  _mech_pattern_get_size (priv->pattern,
                          &width, &width_unit,
                          &height, &height_unit);

  if (axis == MECH_AXIS_X)
    return width;
  else
    return height;
}

static gdouble
mech_image_get_second_extent (MechArea *area,
                              MechAxis  axis,
                              gdouble   other_value)
{
  return mech_image_get_extent (area, axis);
}

static void
mech_image_draw (MechArea *area,
                 cairo_t  *cr)
{
  MechImagePrivate *priv;
  cairo_rectangle_t rect;

  priv = mech_image_get_instance_private ((MechImage *) area);

  if (priv->pattern)
    {
      mech_area_get_allocated_size (area, &rect);
      rect.x = rect.y = 0;

      cairo_rectangle (cr, 0, 0, rect.width, rect.height);
      _mech_pattern_set_source (priv->pattern, cr, &rect);
      cairo_fill (cr);
    }
}

static void
mech_image_class_init (MechImageClass *klass)
{
  MechAreaClass *area_class = MECH_AREA_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = mech_image_set_property;
  object_class->get_property = mech_image_get_property;
  object_class->finalize = mech_image_finalize;

  area_class->get_extent = mech_image_get_extent;
  area_class->get_second_extent = mech_image_get_second_extent;
  area_class->draw = mech_image_draw;

  g_object_class_install_property (object_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "File",
                                                        "The file containing the image",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
mech_image_init (MechImage *image)
{
}

MechArea *
mech_image_new (void)
{
  return g_object_new (MECH_TYPE_IMAGE, NULL);
}

MechArea *
mech_image_new_from_file (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (MECH_TYPE_IMAGE,
                       "file", file,
                       NULL);
}

GFile *
mech_image_get_file (MechImage *image)
{
  MechImagePrivate *priv;

  g_return_val_if_fail (MECH_IS_IMAGE (image), NULL);

  priv = mech_image_get_instance_private (image);
  return priv->file;
}

void
mech_image_set_file (MechImage *image,
                     GFile     *file)
{
  MechImagePrivate *priv;

  g_return_if_fail (MECH_IS_IMAGE (image));

  priv = mech_image_get_instance_private (image);

  if (file)
    g_object_ref (file);

  if (priv->file)
    g_object_unref (priv->file);

  priv->file = file;

  /* Update pattern */
  if (priv->pattern)
    {
      _mech_pattern_unref (priv->pattern);
      priv->pattern = NULL;
    }

  if (file)
    priv->pattern = _mech_pattern_new_asset (file, NULL, CAIRO_EXTEND_NONE);

  mech_area_check_size ((MechArea *) image);
}
