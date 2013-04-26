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

void             _mech_area_make_window_root      (MechArea          *area,
                                                   MechWindow        *window);
gboolean         _mech_area_get_visible_rect      (MechArea          *area,
                                                   cairo_rectangle_t *rect);
gboolean         _mech_area_handle_event          (MechArea          *area,
                                                   MechEvent         *event);
void             _mech_area_get_stage_rect        (MechArea          *area,
                                                   cairo_rectangle_t *rect);

G_END_DECLS

#endif /* __MECH_AREA_PRIVATE_H__ */
