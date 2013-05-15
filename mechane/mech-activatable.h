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

#ifndef __MECH_ACTIVATABLE_H__
#define __MECH_ACTIVATABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_ACTIVATABLE          (mech_activatable_get_type ())
#define MECH_ACTIVATABLE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_ACTIVATABLE, MechActivatable))
#define MECH_IS_ACTIVATABLE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_ACTIVATABLE))
#define MECH_ACTIVATABLE_GET_IFACE(o)  (G_TYPE_INSTANCE_GET_INTERFACE ((o), MECH_TYPE_ACTIVATABLE, MechActivatableInterface))

typedef struct _MechActivatable MechActivatable;
typedef struct _MechActivatableInterface MechActivatableInterface;

struct _MechActivatableInterface
{
  GTypeInterface parent_iface;

  void (* activated) (MechActivatable *activatable);
};

GType mech_activatable_get_type (void) G_GNUC_CONST;
void  mech_activatable_activate (MechActivatable *activatable);

G_END_DECLS

#endif /* __MECH_ACTIVATABLE_H__ */
