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

struct _MechPattern
{
  cairo_pattern_t *pattern;

  gdouble width;
  gdouble height;

  guint width_unit : 3;
  guint height_unit : 3;
  gint ref_count;
};

G_DEFINE_BOXED_TYPE (MechPattern, _mech_pattern,
                     _mech_pattern_ref, _mech_pattern_unref)

MechPattern *
_mech_pattern_new (cairo_pattern_t *pattern)
{
  MechPattern *pat;

  pat = g_new0 (MechPattern, 1);
  pat->pattern = cairo_pattern_reference (pattern);
  pat->ref_count = 1;

  return pat;
}

void
_mech_pattern_set_source (MechPattern             *pattern,
                          cairo_t                 *cr,
                          const cairo_rectangle_t *rect)
{
  cairo_matrix_t matrix;

  cairo_matrix_init_translate (&matrix, -rect->x, -rect->y);
  cairo_pattern_set_matrix (pattern->pattern, &matrix);
  cairo_set_source (cr, pattern->pattern);
}

void
_mech_pattern_render (MechPattern             *pattern,
                      cairo_t                 *cr,
                      const cairo_rectangle_t *rect)
{
  _mech_pattern_set_source (pattern, cr, rect);
  cairo_fill (cr);
}

void
_mech_pattern_set_size (MechPattern *pattern,
			gdouble      width,
			MechUnit     width_unit,
			gdouble      height,
			MechUnit     height_unit)
{
  g_return_if_fail (pattern != NULL);

  pattern->width = width;
  pattern->width_unit= width_unit;
  pattern->height = height;
  pattern->height_unit = height_unit;
}

void
_mech_pattern_get_size (MechPattern *pattern,
                        gdouble     *width,
                        MechUnit    *width_unit,
                        gdouble     *height,
                        MechUnit    *height_unit)
{
  *width = pattern->width;
  *width_unit = pattern->width_unit;
  *height = pattern->height;
  *height_unit = pattern->height_unit;
}

MechPattern *
_mech_pattern_ref (MechPattern *pattern)
{
  g_atomic_int_inc (&pattern->ref_count);
  return pattern;
}

void
_mech_pattern_unref (MechPattern *pattern)
{
  if (g_atomic_int_dec_and_test (&pattern->ref_count))
    {
      cairo_pattern_destroy (pattern->pattern);
      g_free (pattern);
    }
}
