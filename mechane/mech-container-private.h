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

#ifndef __MECH_CONTAINER_PRIVATE_H__
#define __MECH_CONTAINER_PRIVATE_H__

#include <mechane/mech-stage-private.h>
#include <mechane/mech-container.h>

G_BEGIN_DECLS

void         _mech_container_set_surface        (MechContainer *container,
                                                 MechSurface   *surface);
MechStage  * _mech_container_get_stage          (MechContainer *container);
MechCursor * _mech_container_get_current_cursor (MechContainer *container);

G_END_DECLS

#endif /* __MECH_CONTAINER_PRIVATE_H__ */
