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

#ifndef __MECH_CONTROLLER_H__
#define __MECH_CONTROLLER_H__

#include <mechane/mech-events.h>

G_BEGIN_DECLS

#define MECH_TYPE_CONTROLLER         (mech_controller_get_type ())
#define MECH_CONTROLLER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_CONTROLLER, MechController))
#define MECH_CONTROLLER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_CONTROLLER, MechControllerClass))
#define MECH_IS_CONTROLLER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_CONTROLLER))
#define MECH_IS_CONTROLLER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_CONTROLLER))
#define MECH_CONTROLLER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_CONTROLLER, MechControllerClass))

typedef struct _MechController MechController;
typedef struct _MechControllerClass MechControllerClass;

struct _MechController
{
  GObject parent_instance;
};

struct _MechControllerClass
{
  GObjectClass parent_class;

  gboolean (* handle_event) (MechController *controller,
                             MechEvent      *event);
};

GType    mech_controller_get_type     (void) G_GNUC_CONST;

gboolean mech_controller_handle_event (MechController *controller,
                                       MechEvent      *event);

G_END_DECLS

#endif /* __MECH_CONTROLLER_H__ */
