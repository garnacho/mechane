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

#ifndef __MECH_SCROLLABLE_H__
#define __MECH_SCROLLABLE_H__

#include <mechane/mechane.h>
#include <mechane/mech-adjustable.h>

G_BEGIN_DECLS

#define MECH_TYPE_SCROLLABLE          (mech_scrollable_get_type ())
#define MECH_SCROLLABLE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SCROLLABLE, MechScrollable))
#define MECH_IS_SCROLLABLE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SCROLLABLE))
#define MECH_SCROLLABLE_GET_IFACE(o)  (G_TYPE_INSTANCE_GET_INTERFACE ((o), MECH_TYPE_SCROLLABLE, MechScrollableInterface))

typedef struct _MechScrollable MechScrollable;
typedef struct _MechScrollableInterface MechScrollableInterface;

struct _MechScrollableInterface
{
  GTypeInterface parent_iface;

  MechAdjustable * (* get_adjustable) (MechScrollable *scrollable,
                                       MechAxis        axis);
  void             (* set_adjustable) (MechScrollable *scrollable,
                                       MechAxis        axis,
                                       MechAdjustable *adjustable);
};

GType            mech_scrollable_get_type       (void) G_GNUC_CONST;

MechAdjustable * mech_scrollable_get_adjustable (MechScrollable *scrollable,
                                                 MechAxis        axis);
void             mech_scrollable_set_adjustable (MechScrollable *scrollable,
                                                 MechAxis        axis,
                                                 MechAdjustable *adjustable);

G_END_DECLS

#endif /* __MECH_SCROLLABLE_H__ */
