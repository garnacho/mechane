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

#define MECH_TYPE_POINT (mech_point_get_type ())

typedef struct _MechPoint MechPoint;

struct _MechPoint
{
  gdouble x;
  gdouble y;
};

GType        mech_point_get_type    (void) G_GNUC_CONST;
MechPoint *  mech_point_copy        (MechPoint *point);
void         mech_point_free        (MechPoint *point);

G_END_DECLS

#endif /* __MECH_TYPES_H__ */
