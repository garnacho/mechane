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

#ifndef __MECH_AREA_H__
#define __MECH_AREA_H__

typedef struct _MechArea MechArea;

#include <mechane/mech-enums.h>
#include <cairo/cairo.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_AREA         (mech_area_get_type ())
#define MECH_AREA(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_AREA, MechArea))
#define MECH_AREA_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_AREA, MechAreaClass))
#define MECH_IS_AREA(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_AREA))
#define MECH_IS_AREA_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_AREA))
#define MECH_AREA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_AREA, MechAreaClass))

typedef struct _MechAreaClass MechAreaClass;

struct _MechArea
{
  GInitiallyUnowned parent_object;
};

struct _MechAreaClass
{
  GInitiallyUnownedClass parent_class;

  void             (* draw)               (MechArea  *area,
                                           cairo_t   *cr);

  gdouble          (* get_extent)         (MechArea  *area,
                                           MechAxis   axis);
  gdouble          (* get_second_extent)  (MechArea  *area,
                                           MechAxis   axis,
                                           gdouble    other_value);
  void             (* allocate_size)      (MechArea  *area,
                                           gdouble    width,
                                           gdouble    height);

  gboolean         (* child_resize)       (MechArea  *area,
                                           MechArea  *child);
};

GType            mech_area_get_type             (void) G_GNUC_CONST;

gdouble          mech_area_get_extent           (MechArea       *area,
						 MechAxis        axis);
gdouble          mech_area_get_second_extent    (MechArea       *area,
						 MechAxis        axis,
						 gdouble         other_value);

G_END_DECLS

#endif /* __MECH_AREA_H__ */
