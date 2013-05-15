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

#ifndef __MECH_FIXED_BOX_H__
#define __MECH_FIXED_BOX_H__

#include <mechane/mech-area.h>

G_BEGIN_DECLS

#define MECH_TYPE_FIXED_BOX         (mech_fixed_box_get_type ())
#define MECH_FIXED_BOX(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_FIXED_BOX, MechFixedBox))
#define MECH_FIXED_BOX_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_FIXED_BOX, MechFixedBoxClass))
#define MECH_IS_FIXED_BOX(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_FIXED_BOX))
#define MECH_IS_FIXED_BOX_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_FIXED_BOX))
#define MECH_FIXED_BOX_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_FIXED_BOX, MechFixedBoxClass))

typedef struct _MechFixedBox MechFixedBox;
typedef struct _MechFixedBoxClass MechFixedBoxClass;

struct _MechFixedBox
{
  MechArea parent_object;
};

struct _MechFixedBoxClass
{
  MechAreaClass parent_class;
};

GType            mech_fixed_box_get_type       (void) G_GNUC_CONST;
MechArea *       mech_fixed_box_new            (void);

gboolean         mech_fixed_box_child_is_attached (MechFixedBox    *box,
						   MechArea        *area,
						   MechSideFlags    sides);
void             mech_fixed_box_attach            (MechFixedBox    *box,
						   MechArea        *area,
						   MechSide         side,
						   MechSide         attach_side,
						   MechUnit         unit,
						   gdouble          attach);
void             mech_fixed_box_detach            (MechFixedBox    *box,
						   MechArea        *area,
						   MechSide         side);
gboolean         mech_fixed_box_get_attachment    (MechFixedBox    *box,
						   MechArea        *area,
						   MechSide         side,
						   MechUnit         unit,
						   MechSide        *attach_side,
						   gdouble         *attach);

G_END_DECLS

#endif /* __MECH_FIXED_BOX_H__ */
