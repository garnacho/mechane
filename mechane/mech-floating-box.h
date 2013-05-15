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

#ifndef __MECH_FLOATING_BOX_H__
#define __MECH_FLOATING_BOX_H__

#include <mechane/mech-area.h>

G_BEGIN_DECLS

#define MECH_TYPE_FLOATING_BOX         (mech_floating_box_get_type ())
#define MECH_FLOATING_BOX(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_FLOATING_BOX, MechFloatingBox))
#define MECH_FLOATING_BOX_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_FLOATING_BOX, MechFloatingBoxClass))
#define MECH_IS_FLOATING_BOX(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_FLOATING_BOX))
#define MECH_IS_FLOATING_BOX_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_FLOATING_BOX))
#define MECH_FLOATING_BOX_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_FLOATING_BOX, MechFloatingBoxClass))

typedef struct _MechFloatingBox MechFloatingBox;
typedef struct _MechFloatingBoxClass MechFloatingBoxClass;

struct _MechFloatingBox
{
  MechArea parent_object;
};

struct _MechFloatingBoxClass
{
  MechAreaClass parent_class;
};

GType            mech_floating_box_get_type       (void) G_GNUC_CONST;
MechArea *       mech_floating_box_new            (void);

G_END_DECLS

#endif /* __MECH_FLOATING_BOX_H__ */
