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

#include <pango/pangocairo.h>
#include <pango/pango.h>
#include <string.h>
#include <mechane/mechane.h>
#include "mech-renderer-private.h"
#include "mech-renderer.h"

typedef struct _MechRendererPrivate MechRendererPrivate;

struct _MechRendererPrivate
{
  PangoContext *font_context;
  PangoFontDescription *font_desc;

  MechBorder margin;
  MechBorder padding;

  GArray *foregrounds;
  GArray *backgrounds;
};

G_DEFINE_TYPE_WITH_PRIVATE (MechRenderer, mech_renderer, G_TYPE_OBJECT)

static void
mech_renderer_finalize (GObject *object)
{
  MechRendererPrivate *priv;
  gint i;

  priv = mech_renderer_get_instance_private ((MechRenderer *) object);
  pango_font_description_free (priv->font_desc);
  g_object_unref (priv->font_context);

  for (i = 0; i < priv->foregrounds->len; i++)
    _mech_pattern_unref (g_array_index (priv->foregrounds, MechPattern *, i));

  for (i = 0; i < priv->backgrounds->len; i++)
    _mech_pattern_unref (g_array_index (priv->backgrounds, MechPattern *, i));

  g_array_unref (priv->foregrounds);
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
  priv->foregrounds = g_array_new (FALSE, FALSE, sizeof (MechPattern *));
  priv->backgrounds = g_array_new (FALSE, FALSE, sizeof (MechPattern *));

  priv->margin.left = priv->margin.right = 0;
  priv->margin.top = priv->margin.bottom = 0;
  priv->padding.left = priv->padding.right = 0;
  priv->padding.top = priv->padding.bottom = 0;

  font_map = pango_cairo_font_map_get_default ();
  priv->font_context = pango_font_map_create_context (font_map);

  priv->font_desc = pango_font_description_from_string ("Cantarell 10");
  pango_context_set_font_description (priv->font_context, priv->font_desc);
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

gint
_mech_renderer_add_foreground (MechRenderer *renderer,
                               MechPattern  *pattern)
{
  MechRendererPrivate *priv;

  g_return_val_if_fail (MECH_IS_RENDERER (renderer), -1);
  g_return_val_if_fail (pattern != NULL, -1);

  priv = mech_renderer_get_instance_private (renderer);
  _mech_pattern_ref (pattern);
  g_array_append_val (priv->foregrounds, pattern);

  return priv->foregrounds->len - 1;
}

void
_mech_renderer_set_font_family (MechRenderer *renderer,
                                const gchar  *family)
{
  MechRendererPrivate *priv;

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (family != NULL);

  priv = mech_renderer_get_instance_private (renderer);
  pango_font_description_set_family (priv->font_desc, family);
  pango_context_set_font_description (priv->font_context, priv->font_desc);
}

void
_mech_renderer_set_font_size (MechRenderer *renderer,
                              gdouble       size,
                              MechUnit      unit)
{
  MechRendererPrivate *priv;

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (size > 0);

  priv = mech_renderer_get_instance_private (renderer);

  if (unit == MECH_UNIT_PT)
    pango_font_description_set_size (priv->font_desc,
                                     (int) size * PANGO_SCALE);
  else if (unit == MECH_UNIT_PX)
    pango_font_description_set_absolute_size (priv->font_desc,
                                              (int) size * PANGO_SCALE);
  else
    {
      GEnumClass *enum_class;
      GEnumValue *enum_value;

      enum_class = g_type_class_ref (MECH_TYPE_UNIT);
      enum_value = g_enum_get_value (enum_class, unit);
      g_warning ("Unit '%s' unsupported on font sizes",
                 enum_value->value_nick);
      g_type_class_unref (enum_class);
      return;
    }

  pango_context_set_font_description (priv->font_context, priv->font_desc);
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

void
mech_renderer_render_layout (MechRenderer *renderer,
                             cairo_t      *cr,
                             PangoLayout  *layout)
{
  cairo_rectangle_t rect = { 0 };
  MechRendererPrivate *priv;
  PangoRectangle ink_rect;
  MechPattern *pattern;
  guint i;

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (cr != NULL);

  priv = mech_renderer_get_instance_private (renderer);

  if (priv->foregrounds->len == 0)
    return;

  cairo_save (cr);

  pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
  rect.x = ink_rect.x;
  rect.y = ink_rect.y;
  rect.width = ink_rect.width;
  rect.height = ink_rect.height;

  if (G_LIKELY (priv->foregrounds->len == 1))
    {
      pattern = g_array_index (priv->foregrounds, MechPattern *, 0);
      _mech_pattern_set_source (pattern, cr, &rect);
      pango_cairo_show_layout (cr, layout);
    }
  else
    {
      pango_cairo_layout_path (cr, layout);

      for (i = 0; i < priv->foregrounds->len; i++)
        {
          pattern = g_array_index (priv->foregrounds, MechPattern *, i);
          _mech_pattern_set_source (pattern, cr, &rect);

          if (i < priv->foregrounds->len - 1)
            cairo_fill_preserve (cr);
          else
            cairo_fill (cr);
        }
    }

  cairo_restore (cr);
}

PangoLayout *
mech_renderer_create_layout (MechRenderer *renderer,
                             const gchar  *text)
{
  MechRendererPrivate *priv;
  PangoLayout *layout;

  g_return_val_if_fail (MECH_IS_RENDERER (renderer), NULL);

  priv = mech_renderer_get_instance_private (renderer);
  layout = pango_layout_new (priv->font_context);

  if (text)
    pango_layout_set_text (layout, text, -1);

  return layout;
}

PangoContext *
mech_renderer_get_font_context (MechRenderer *renderer)
{
  MechRendererPrivate *priv;

  g_return_val_if_fail (MECH_IS_RENDERER (renderer), NULL);

  priv = mech_renderer_get_instance_private (renderer);
  return priv->font_context;
}
