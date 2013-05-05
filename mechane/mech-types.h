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

#ifndef __MECH_TYPES_H__
#define __MECH_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_COLOR (mech_color_get_type ())
#define MECH_TYPE_POINT (mech_point_get_type ())
#define MECH_TYPE_BORDER (mech_border_get_type ())

typedef struct _MechColor MechColor;
typedef struct _MechPoint MechPoint;
typedef struct _MechBorder MechBorder;

struct _MechColor
{
  gdouble r;
  gdouble g;
  gdouble b;
  gdouble a;
};

struct _MechPoint
{
  gdouble x;
  gdouble y;
};

struct _MechBorder
{
  gdouble left;
  gdouble right;
  gdouble top;
  gdouble bottom;

  guint left_unit   : 4;
  guint right_unit  : 4;
  guint top_unit    : 4;
  guint bottom_unit : 4;
};

GType        mech_color_get_type    (void) G_GNUC_CONST;
MechColor *  mech_color_copy        (MechColor *color);
void         mech_color_free        (MechColor *color);

GType        mech_point_get_type    (void) G_GNUC_CONST;
MechPoint *  mech_point_copy        (MechPoint *point);
void         mech_point_free        (MechPoint *point);

GType        mech_border_get_type   (void) G_GNUC_CONST;
MechBorder * mech_border_copy       (MechBorder *border);
void         mech_border_free       (MechBorder *border);

G_END_DECLS

#endif /* __MECH_TYPES_H__ */
