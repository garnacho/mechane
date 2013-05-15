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

#ifndef __MECH_BUTTON_H__
#define __MECH_BUTTON_H__

#include <mechane/mech-area.h>

G_BEGIN_DECLS

#define MECH_TYPE_BUTTON         (mech_button_get_type ())
#define MECH_BUTTON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_BUTTON, MechButton))
#define MECH_BUTTON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_BUTTON, MechButtonClass))
#define MECH_IS_BUTTON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_BUTTON))
#define MECH_IS_BUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_BUTTON))
#define MECH_BUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_BUTTON, MechButtonClass))

typedef struct _MechButton MechButton;
typedef struct _MechButtonClass MechButtonClass;

struct _MechButton
{
  MechArea parent_instance;
};

struct _MechButtonClass
{
  MechAreaClass parent_class;
};

GType            mech_button_get_type (void) G_GNUC_CONST;
MechArea *       mech_button_new      (void);

G_END_DECLS

#endif /* __MECH_BUTTON_H__ */
