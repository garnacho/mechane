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

#include <cairo/cairo.h>
#include <glib-object.h>
#include <mechane/mech-enums.h>
#include <mechane/mech-window.h>
#include <mechane/mech-types.h>
#include <mechane/mech-events.h>

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
  gboolean         (* handle_event)       (MechArea  *area,
                                           MechEvent *event);

  void             (* add)                (MechArea  *area,
                                           MechArea  *child);
  void             (* remove)             (MechArea  *area,
                                           MechArea  *child);

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

  cairo_region_t * (* get_shape)          (MechArea  *area);

  void             (* visibility_changed) (MechArea  *area);
};

GType            mech_area_get_type             (void) G_GNUC_CONST;

void             mech_area_add                  (MechArea       *area,
						 MechArea       *child);
void             mech_area_remove               (MechArea       *area,
						 MechArea       *child);
gint             mech_area_get_children         (MechArea       *area,
						 MechArea     ***children);

gboolean         mech_area_is_ancestor          (MechArea       *area,
						 MechArea       *ancestor);

void             mech_area_set_matrix           (MechArea             *area,
                                                 const cairo_matrix_t *matrix);
gboolean         mech_area_get_matrix           (MechArea             *area,
                                                 cairo_matrix_t       *matrix);
gboolean         mech_area_get_relative_matrix  (MechArea             *area,
                                                 MechArea             *relative_to,
                                                 cairo_matrix_t       *matrix_ret);

gdouble          mech_area_get_extent           (MechArea       *area,
						 MechAxis        axis);
gdouble          mech_area_get_second_extent    (MechArea       *area,
						 MechAxis        axis,
						 gdouble         other_value);

MechArea       * mech_area_get_parent           (MechArea       *area);
void             mech_area_set_parent           (MechArea       *area,
						 MechArea       *parent);

MechWindow     * mech_area_get_window           (MechArea       *area);

void             mech_area_set_clip             (MechArea        *area,
                                                 gboolean         clip);
gboolean         mech_area_get_clip             (MechArea        *area);

void             mech_area_set_events           (MechArea        *area,
                                                 MechEventMask    evmask);
void             mech_area_add_events           (MechArea        *area,
                                                 MechEventMask    evmask);

MechEventMask    mech_area_get_events           (MechArea        *area);
gboolean         mech_area_handles_event        (MechArea        *area,
                                                 MechEventType    event_type);

void             mech_area_allocate_size        (MechArea          *area,
						 cairo_rectangle_t *size);
void             mech_area_get_allocated_size   (MechArea          *area,
						 cairo_rectangle_t *size);

void             mech_area_transform_points     (MechArea          *area,
                                                 MechArea          *relative_to,
                                                 MechPoint         *points,
                                                 gint               n_points);
void             mech_area_transform_point      (MechArea          *area,
                                                 MechArea          *relative_to,
                                                 gdouble           *x,
                                                 gdouble           *y);
void             mech_area_transform_corners    (MechArea          *area,
                                                 MechArea          *relative_to,
                                                 MechPoint         *top_left,
                                                 MechPoint         *top_right,
                                                 MechPoint         *bottom_left,
                                                 MechPoint         *bottom_right);
void             mech_area_get_extents          (MechArea          *area,
                                                 MechArea          *relative_to,
                                                 cairo_rectangle_t *extents);

void             mech_area_set_visible          (MechArea          *area,
						 gboolean           visible);
gboolean         mech_area_get_visible          (MechArea          *area);
gboolean         mech_area_is_visible           (MechArea          *area);

gint             mech_area_get_depth            (MechArea          *area);
void             mech_area_set_depth            (MechArea          *area,
                                                 gint               depth);

cairo_region_t  * mech_area_get_shape           (MechArea          *area);

G_END_DECLS

#endif /* __MECH_AREA_H__ */
