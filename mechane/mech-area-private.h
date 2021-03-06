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

#ifndef __MECH_AREA_PRIVATE_H__
#define __MECH_AREA_PRIVATE_H__

#include <mechane/mech-stage-private.h>
#include <mechane/mech-area.h>

G_BEGIN_DECLS

MechStage      * _mech_area_get_stage             (MechArea          *area);
GNode          * _mech_area_get_node              (MechArea          *area);

void             _mech_area_set_window            (MechArea          *area,
                                                   MechWindow        *window);
void             _mech_area_set_container         (MechArea          *area,
                                                   MechContainer     *container);
MechContainer *  _mech_area_get_container         (MechArea          *area);

gboolean         _mech_area_handle_event          (MechArea          *area,
                                                   MechEvent         *event);
gboolean         _mech_area_get_visible_rect      (MechArea          *area,
                                                   cairo_rectangle_t *rect);
gboolean         _mech_area_get_renderable_rect   (MechArea          *area,
                                                   cairo_rectangle_t *rect);
cairo_region_t * _mech_area_get_renderable_region (MechArea          *area);
void             _mech_area_guess_offscreen_size  (MechArea          *area,
                                                   gint              *width,
                                                   gint              *height);
void             _mech_area_get_stage_rect        (MechArea          *area,
                                                   cairo_rectangle_t *rect);
void             _mech_area_reset_surface_type    (MechArea          *area,
                                                   MechSurfaceType    surface_type);
void             _mech_area_notify_visibility_change (MechArea       *area);



G_END_DECLS

#endif /* __MECH_AREA_PRIVATE_H__ */
