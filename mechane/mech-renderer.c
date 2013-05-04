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

#define MIN4(a,b,c,d) MIN (MIN ((a), (b)), MIN ((c), (d)))
#define NORMALIZE_CORNERS(s) G_STMT_START { \
    if (((s) & (MECH_SIDE_FLAG_LEFT | MECH_SIDE_FLAG_RIGHT)) && \
        !((s) & (MECH_SIDE_FLAG_TOP | MECH_SIDE_FLAG_BOTTOM)))  \
      (s) |= (MECH_SIDE_FLAG_TOP | MECH_SIDE_FLAG_BOTTOM);      \
    if (((s) & (MECH_SIDE_FLAG_TOP | MECH_SIDE_FLAG_BOTTOM)) && \
        !((s) & (MECH_SIDE_FLAG_LEFT | MECH_SIDE_FLAG_RIGHT)))  \
      (s) |= (MECH_SIDE_FLAG_LEFT | MECH_SIDE_FLAG_RIGHT);      \
  } G_STMT_END

#define ADD_BORDER(a,b)                         \
  G_STMT_START {                                \
    (a)->left += (b)->left;                     \
    (a)->right += (b)->right;                   \
    (a)->top += (b)->top;                       \
    (a)->bottom += (b)->bottom;                 \
  } G_STMT_END

typedef struct _MechRendererPrivate MechRendererPrivate;
typedef struct _BorderData BorderData;

enum {
  MECH_CORNER_TOP_LEFT,
  MECH_CORNER_TOP_RIGHT,
  MECH_CORNER_BOTTOM_RIGHT,
  MECH_CORNER_BOTTOM_LEFT
};

struct _BorderData
{
  MechPattern *pattern;
  MechBorder border;
};

struct _MechRendererPrivate
{
  PangoContext *font_context;
  PangoFontDescription *font_desc;

  MechBorder margin;
  MechBorder padding;
  MechBorder border;

  GArray *foregrounds;
  GArray *backgrounds;
  GArray *borders;
  gdouble radii[4];
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

  for (i = 0; i < priv->borders->len; i++)
    {
      BorderData *data = &g_array_index (priv->borders, BorderData, i);
      _mech_pattern_unref (data->pattern);
    }

  g_array_unref (priv->foregrounds);
  g_array_unref (priv->backgrounds);
  g_array_unref (priv->borders);

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
  priv->borders = g_array_new (FALSE, FALSE, sizeof (BorderData));

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

MechRenderer *
_mech_renderer_copy (MechRenderer          *renderer,
                     MechRendererCopyFlags  flags)
{
  MechRendererPrivate *priv, *copy_priv;
  MechRenderer *copy;
  guint i;

  copy = _mech_renderer_new ();
  priv = mech_renderer_get_instance_private (renderer);
  copy_priv = mech_renderer_get_instance_private (copy);

  if (flags & MECH_RENDERER_COPY_BACKGROUND)
    {
      g_array_append_vals (copy_priv->backgrounds,
                           priv->backgrounds->data,
                           priv->backgrounds->len);

      for (i = 0; i < copy_priv->backgrounds->len; i++)
        _mech_pattern_ref (g_array_index (copy_priv->backgrounds,
                                          MechPattern *, i));
    }

  if (flags & MECH_RENDERER_COPY_FOREGROUND)
    {
      g_object_unref (copy_priv->font_context);
      copy_priv->font_context = g_object_ref (priv->font_context);

      pango_font_description_free (copy_priv->font_desc);
      copy_priv->font_desc = pango_font_description_copy (priv->font_desc);

      g_array_append_vals (copy_priv->foregrounds,
                           priv->foregrounds->data,
                           priv->foregrounds->len);

      for (i = 0; i < copy_priv->foregrounds->len; i++)
        _mech_pattern_ref (g_array_index (copy_priv->foregrounds,
                                          MechPattern *, i));
    }

  if (flags & MECH_RENDERER_COPY_BORDER)
    {
      copy_priv->margin = priv->margin;
      copy_priv->padding = priv->padding;
      memcpy (copy_priv->radii, priv->radii,
              sizeof (double) * G_N_ELEMENTS (priv->radii));

      g_array_append_vals (copy_priv->borders,
                           priv->borders->data,
                           priv->borders->len);

      for (i = 0; i < copy_priv->borders->len; i++)
        {
          BorderData *border;

          border = &g_array_index (priv->borders, BorderData, i);
          _mech_pattern_ref (border->pattern);
        }
    }

  return copy;
}

static void
_mech_renderer_rounded_rectangle (MechRenderer *renderer,
                                  cairo_t      *cr,
                                  gdouble       x,
                                  gdouble       y,
                                  gdouble       width,
                                  gdouble       height,
                                  gdouble       radius_diff)
{
  gdouble radius, max_radius;
  MechRendererPrivate *priv;

  priv = mech_renderer_get_instance_private (renderer);
  max_radius = MIN (width / 2, height / 2);

  /* Top border */
  radius = CLAMP (priv->radii[MECH_CORNER_TOP_LEFT] - radius_diff,
                  0, max_radius);
  cairo_move_to (cr, x + radius, y);

  radius = CLAMP (priv->radii[MECH_CORNER_TOP_RIGHT] - radius_diff,
                  0, max_radius);
  cairo_line_to (cr, x + width - radius, y);

  /* Top/right corner */
  if (radius > 0)
    cairo_arc (cr, x + width - radius, y + radius, radius, 3 * G_PI / 2, 0);

  radius = CLAMP (priv->radii[MECH_CORNER_BOTTOM_RIGHT] - radius_diff,
                  0, max_radius);

  /* Right border */
  cairo_line_to (cr, x + width, y + height - radius);

  /* Bottom/right corner */
  if (radius > 0)
    cairo_arc (cr, x + width - radius,
               y + height - radius, radius, 0, G_PI / 2);

  radius = CLAMP (priv->radii[MECH_CORNER_BOTTOM_LEFT] - radius_diff,
                  0, max_radius);

  /* Bottom border */
  cairo_line_to (cr, x + radius, y + height);

  /* Bottom/left corner */
  if (radius > 0)
    cairo_arc (cr, x + radius, y + height - radius, radius, G_PI / 2, G_PI);

  radius = CLAMP (priv->radii[MECH_CORNER_TOP_LEFT] - radius_diff,
                  0, max_radius);

  /* Left border */
  cairo_line_to (cr, x, y + radius);

  /* Top/left corner */
  if (radius > 0)
    cairo_arc (cr, x + radius, y + radius, radius, G_PI, 3 * G_PI / 2);

  cairo_close_path (cr);
}

static void
_mech_renderer_apply_background (MechRenderer            *renderer,
                                 cairo_t                 *cr,
                                 MechPattern             *pattern,
                                 const cairo_rectangle_t *rect,
                                 gdouble                  radius_diff)
{
  _mech_renderer_rounded_rectangle (renderer, cr,
                                    rect->x, rect->y,
                                    rect->width, rect->height,
                                    radius_diff);
  _mech_pattern_render (pattern, cr, rect);
}

static void
_mech_renderer_apply_border (MechRenderer      *renderer,
                             cairo_t           *cr,
                             BorderData        *border,
                             cairo_rectangle_t *rect,
                             gdouble           *radius_diff)
{
  cairo_set_line_width (cr, 1);
  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);

  _mech_renderer_rounded_rectangle (renderer, cr,
                                    rect->x, rect->y,
                                    rect->width, rect->height,
                                    *radius_diff);
  _mech_pattern_set_source (border->pattern, cr, rect);

  /* FIXME: deal with unit */
  rect->x += border->border.left;
  rect->y += border->border.top;
  rect->width -= border->border.left + border->border.right;
  rect->height -= border->border.top + border->border.bottom;

  *radius_diff += MIN4 (border->border.left, border->border.right,
                        border->border.top, border->border.bottom);

  _mech_renderer_rounded_rectangle (renderer, cr,
                                    rect->x, rect->y,
                                    rect->width, rect->height,
                                    *radius_diff);
  cairo_fill (cr);
}

void
_mech_renderer_set_corner_radius (MechRenderer *renderer,
                                  guint         corners,
                                  gdouble       radius)
{
  MechRendererPrivate *priv;

  g_return_if_fail (MECH_IS_RENDERER (renderer));

  priv = mech_renderer_get_instance_private (renderer);
  NORMALIZE_CORNERS (corners);

  if ((corners & MECH_SIDE_FLAG_CORNER_TOP_LEFT) == MECH_SIDE_FLAG_CORNER_TOP_LEFT)
    priv->radii[MECH_CORNER_TOP_LEFT] = radius;
  if ((corners & MECH_SIDE_FLAG_CORNER_TOP_RIGHT) == MECH_SIDE_FLAG_CORNER_TOP_RIGHT)
    priv->radii[MECH_CORNER_TOP_RIGHT] = radius;
  if ((corners & MECH_SIDE_FLAG_CORNER_BOTTOM_LEFT) == MECH_SIDE_FLAG_CORNER_BOTTOM_LEFT)
    priv->radii[MECH_CORNER_BOTTOM_LEFT] = radius;
  if ((corners & MECH_SIDE_FLAG_CORNER_BOTTOM_RIGHT) == MECH_SIDE_FLAG_CORNER_BOTTOM_RIGHT)
    priv->radii[MECH_CORNER_BOTTOM_RIGHT] = radius;
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
_mech_renderer_add_border (MechRenderer *renderer,
                           MechPattern  *pattern,
                           MechBorder   *border)
{
  MechRendererPrivate *priv;
  BorderData data;

  g_return_val_if_fail (MECH_IS_RENDERER (renderer), -1);
  g_return_val_if_fail (pattern != NULL, -1);
  g_return_val_if_fail (border != NULL, -1);

  priv = mech_renderer_get_instance_private (renderer);
  data.pattern = _mech_pattern_ref (pattern);
  data.border = *border;
  g_array_append_val (priv->borders, data);

  ADD_BORDER (&priv->border, border);

  return priv->borders->len - 1;
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
mech_renderer_set_border_path (MechRenderer *renderer,
                               cairo_t      *cr,
                               gdouble       x,
                               gdouble       y,
                               gdouble       width,
                               gdouble       height)
{
  cairo_rectangle_t border;

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (cr != NULL);

  border.x = x;
  border.y = y;
  border.width = width;
  border.height = height;

  _mech_renderer_rounded_rectangle (renderer, cr, border.x, border.y,
                                    border.width, border.height, 0);
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
  gdouble radius;
  guint i;

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (cr != NULL);

  priv = mech_renderer_get_instance_private (renderer);
  radius = MIN4 (priv->border.left, priv->border.right,
                 priv->border.top, priv->border.bottom);

  cairo_save (cr);

  for (i = 0; i < priv->backgrounds->len; i++)
    {
      pattern = g_array_index (priv->backgrounds, MechPattern *, i);
      _mech_renderer_apply_background (renderer, cr, pattern, &rect, radius);
    }

  cairo_restore (cr);
}

void
mech_renderer_render_border (MechRenderer *renderer,
                             cairo_t      *cr,
                             gdouble       x,
                             gdouble       y,
                             gdouble       width,
                             gdouble       height)
{
  MechRendererPrivate *priv;
  cairo_rectangle_t border;
  BorderData *border_data;
  gdouble radius_diff = 0;
  guint i;

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (cr != NULL);

  priv = mech_renderer_get_instance_private (renderer);
  border.x = x;
  border.y = y;
  border.width = width;
  border.height = height;

  cairo_save (cr);

  for (i = 0; i < priv->borders->len; i++)
    {
      border_data = &g_array_index (priv->borders, BorderData, i);
      _mech_renderer_apply_border (renderer, cr, border_data,
                                   &border, &radius_diff);
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

void
mech_renderer_get_border_extents (MechRenderer   *renderer,
                                  MechExtentType  type,
                                  MechBorder     *border)
{
  MechRendererPrivate *priv;
  MechBorder accum = { 0 };

  g_return_if_fail (MECH_IS_RENDERER (renderer));
  g_return_if_fail (border != NULL);

  /* FIXME: deal with units */
  priv = mech_renderer_get_instance_private (renderer);

  if (type > MECH_EXTENT_MARGIN)
    ADD_BORDER (&accum, &priv->margin);

  if (type > MECH_EXTENT_BORDER)
    ADD_BORDER (&accum, &priv->border);

  if (type > MECH_EXTENT_PADDING)
    ADD_BORDER (&accum, &priv->padding);

  *border = accum;
}
