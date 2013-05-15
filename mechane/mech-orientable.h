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

#ifndef __MECH_ORIENTABLE_H__
#define __MECH_ORIENTABLE_H__

#include <mechane/mechane.h>

G_BEGIN_DECLS

#define MECH_TYPE_ORIENTABLE         (mech_orientable_get_type ())
#define MECH_ORIENTABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_ORIENTABLE, MechOrientable))
#define MECH_ORIENTABLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), MECH_TYPE_ORIENTABLE, MechOrientableIface))
#define MECH_IS_ORIENTABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_ORIENTABLE))
#define MECH_IS_ORIENTABLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), MECH_TYPE_ORIENTABLE))
#define MECH_ORIENTABLE_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), MECH_TYPE_ORIENTABLE, MechOrientableIface))

typedef struct _MechOrientable MechOrientable;
typedef struct _MechOrientableInterface MechOrientableInterface;

struct _MechOrientableInterface
{
  GTypeInterface g_iface;
};

GType           mech_orientable_get_type (void) G_GNUC_CONST;

MechOrientation mech_orientable_get_orientation (MechOrientable  *orientable);
void            mech_orientable_set_orientation (MechOrientable  *orientable,
                                                 MechOrientation  orientation);

G_END_DECLS

#endif /* __MECH_ORIENTABLE_H__ */
