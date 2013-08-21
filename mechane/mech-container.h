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

#ifndef __MECH_CONTAINER_H__
#define __MECH_CONTAINER_H__

#include <mechane/mech-area.h>
#include <mechane/mech-events.h>

G_BEGIN_DECLS

#define MECH_TYPE_CONTAINER         (mech_container_get_type ())
#define MECH_CONTAINER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_CONTAINER, MechContainer))
#define MECH_CONTAINER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_CONTAINER, MechContainerClass))
#define MECH_IS_CONTAINER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_CONTAINER))
#define MECH_IS_CONTAINER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_CONTAINER))
#define MECH_CONTAINER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_CONTAINER, MechContainerClass))

typedef struct _MechContainer MechContainer;
typedef struct _MechContainerClass MechContainerClass;

struct _MechContainer
{
  GObject parent_instance;
};

struct _MechContainerClass
{
  GObjectClass parent_class;

  MechArea * (* create_root)     (MechContainer  *container);
};

GType      mech_container_get_type        (void) G_GNUC_CONST;

MechArea * mech_container_get_root        (MechContainer *container);

G_END_DECLS

#endif /* __MECH_CONTAINER_H__ */
