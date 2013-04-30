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

#include <mechane/mechane.h>
#include "mech-renderer-private.h"
#include "mech-renderer.h"

typedef struct _MechRendererPrivate MechRendererPrivate;

struct _MechRendererPrivate
{
  MechBorder margin;
  MechBorder padding;

  GArray *backgrounds;
};

G_DEFINE_TYPE_WITH_PRIVATE (MechRenderer, mech_renderer, G_TYPE_OBJECT)

static void
mech_renderer_finalize (GObject *object)
{
  MechRendererPrivate *priv;
  gint i;

  priv = mech_renderer_get_instance_private ((MechRenderer *) object);

  for (i = 0; i < priv->backgrounds->len; i++)
    _mech_pattern_unref (g_array_index (priv->backgrounds, MechPattern *, i));

  g_array_unref (priv->backgrounds);

  G_OBJECT_CLASS (mech_renderer_parent_class)->finalize (object);
}

static void
mech_renderer_class_init (MechRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mech_renderer_finalize;
}

static void
mech_renderer_init (MechRenderer *renderer)
{
  MechRendererPrivate *priv;
  PangoFontMap *font_map;

  priv = mech_renderer_get_instance_private (renderer);
  priv->backgrounds = g_array_new (FALSE, FALSE, sizeof (MechPattern *));

  priv->margin.left = priv->margin.right = 0;
  priv->margin.top = priv->margin.bottom = 0;
  priv->padding.left = priv->padding.right = 0;
  priv->padding.top = priv->padding.bottom = 0;
}

MechRenderer *
_mech_renderer_new (void)
{
  return g_object_new (MECH_TYPE_RENDERER, NULL);
}

gint
_mech_renderer_add_background (MechRenderer *renderer,
                               MechPattern  *pattern)
{
  MechRendererPrivate *priv;

  g_return_val_if_fail (MECH_IS_RENDERER (renderer), -1);
  g_return_val_if_fail (pattern != NULL, -1);

  priv = mech_renderer_get_instance_private (renderer);
  _mech_pattern_ref (pattern);
  g_array_append_val (priv->backgrounds, pattern);

  return priv->backgrounds->len - 1;
}

void
_mech_renderer_set_padding (MechRenderer *renderer,
                            MechBorder   *border)
{
  MechRendererPrivate *priv;

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (border != NULL);

  priv = mech_renderer_get_instance_private (renderer);
  priv->padding = *border;
}

void
_mech_renderer_set_margin (MechRenderer *renderer,
                           MechBorder   *border)
{
  MechRendererPrivate *priv;

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (border != NULL);

  priv = mech_renderer_get_instance_private (renderer);
  priv->margin = *border;
}

void
mech_renderer_render_background (MechRenderer *renderer,
                                 cairo_t      *cr,
                                 gdouble       x,
                                 gdouble       y,
                                 gdouble       width,
                                 gdouble       height)
{
  cairo_rectangle_t rect = { x, y, width, height };
  MechRendererPrivate *priv;
  MechPattern *pattern;
  guint i;

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (cr != NULL);

  priv = mech_renderer_get_instance_private (renderer);

  cairo_save (cr);

  for (i = 0; i < priv->backgrounds->len; i++)
    {
      pattern = g_array_index (priv->backgrounds, MechPattern *, i);
      cairo_rectangle (cr, x, y, width, height);
      _mech_pattern_render (pattern, cr, &rect);
      cairo_fill (cr);
    }

  cairo_restore (cr);
}
