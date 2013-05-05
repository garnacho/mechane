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

#ifndef __MECH_PATTERN_PRIVATE_H__
#define __MECH_PATTERN_PRIVATE_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <cairo.h>
#include <mechane/mech-enums.h>

G_BEGIN_DECLS

#define MECH_TYPE_PATTERN (_mech_pattern_get_type ())

typedef struct _MechPattern MechPattern;

GType         _mech_pattern_get_type    (void) G_GNUC_CONST;

MechPattern * _mech_pattern_new         (cairo_pattern_t         *pattern);
MechPattern * _mech_pattern_new_asset   (GFile                   *file,
                                         const gchar             *layer,
                                         cairo_extend_t           extend);

void          _mech_pattern_set_size    (MechPattern             *pattern,
                                         gdouble                  width,
                                         MechUnit                 width_unit,
                                         gdouble                  height,
                                         MechUnit                 height_unit);
void          _mech_pattern_set_source  (MechPattern             *pattern,
                                         cairo_t                 *cr,
                                         const cairo_rectangle_t *rect);
void          _mech_pattern_render      (MechPattern             *pattern,
                                         cairo_t                 *cr,
                                         const cairo_rectangle_t *rect);

void          _mech_pattern_get_size    (MechPattern             *pattern,
                                         gdouble                 *width,
                                         MechUnit                *width_unit,
                                         gdouble                 *height,
                                         MechUnit                *height_unit);

MechPattern * _mech_pattern_ref         (MechPattern             *pattern);
void          _mech_pattern_unref       (MechPattern             *pattern);

G_END_DECLS

#endif /* __MECH_PATTERN_PRIVATE_H__ */
