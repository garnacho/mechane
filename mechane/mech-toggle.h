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

#ifndef __MECH_TOGGLE_H__
#define __MECH_TOGGLE_H__

#include <mechane/mech-activatable.h>

G_BEGIN_DECLS

#define MECH_TYPE_TOGGLE          (mech_toggle_get_type ())
#define MECH_TOGGLE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_TOGGLE, MechToggle))
#define MECH_IS_TOGGLE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_TOGGLE))
#define MECH_TOGGLE_GET_IFACE(o)  (G_TYPE_INSTANCE_GET_INTERFACE ((o), MECH_TYPE_TOGGLE, MechToggleInterface))

typedef struct _MechToggle MechToggle;
typedef struct _MechToggleInterface MechToggleInterface;

struct _MechToggleInterface
{
  MechActivatableInterface parent_iface;

  void (*toggled) (MechToggle *toggle);
};

GType    mech_toggle_get_type   (void) G_GNUC_CONST;

gboolean mech_toggle_get_active (MechToggle *toggle);
void     mech_toggle_set_active (MechToggle *toggle,
                                 gboolean    active);

G_END_DECLS

#endif /* __MECH_TOGGLE_H__ */
