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

#ifndef __MECH_VIEW_H__
#define __MECH_VIEW_H__

#include <mechane/mech-area.h>

G_BEGIN_DECLS

#define MECH_TYPE_VIEW         (mech_view_get_type ())
#define MECH_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_VIEW, MechView))
#define MECH_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_VIEW, MechViewClass))
#define MECH_IS_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_VIEW))
#define MECH_IS_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_VIEW))
#define MECH_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_VIEW, MechViewClass))

typedef struct _MechView MechView;
typedef struct _MechViewClass MechViewClass;

struct _MechView
{
  MechArea parent_instance;
};

struct _MechViewClass
{
  MechAreaClass parent_class;

  void (* viewport_moved) (MechView       *view,
                           cairo_region_t *visible,
                           cairo_region_t *previous);

  void (* get_boundaries) (MechView *view,
                           MechAxis  axis,
                           gdouble  *min,
                           gdouble  *max);
};

GType   mech_view_get_type       (void) G_GNUC_CONST;

void    mech_view_set_position   (MechView *view,
                                  gdouble   x,
                                  gdouble   y);
gdouble mech_view_get_boundaries (MechView *view,
                                  MechAxis  axis,
                                  gdouble  *min,
                                  gdouble  *max);

G_END_DECLS

#endif /* __MECH_VIEW_H__ */
