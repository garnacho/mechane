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

#ifndef __MECH_ACCELERATION_H__
#define __MECH_ACCELERATION_H__

#include "mech-animation.h"
#include "mech-enums.h"

G_BEGIN_DECLS

#define MECH_TYPE_ACCELERATION         (mech_acceleration_get_type ())
#define MECH_ACCELERATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_ACCELERATION, MechAcceleration))
#define MECH_ACCELERATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_ACCELERATION, MechAccelerationClass))
#define MECH_IS_ACCELERATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_ACCELERATION))
#define MECH_IS_ACCELERATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_ACCELERATION))
#define MECH_ACCELERATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_ACCELERATION, MechAccelerationClass))

typedef struct _MechAcceleration MechAcceleration;
typedef struct _MechAccelerationClass MechAccelerationClass;

struct _MechAcceleration
{
  MechAnimation parent_instance;
};

struct _MechAccelerationClass
{
  MechAnimationClass parent_class;

  void     (* displaced)   (MechAcceleration *acceleration,
                            gdouble           offset,
                            gdouble           velocity);
  void     (* stopped)     (MechAcceleration *acceleration,
                            gint64            _time,
                            gdouble           offset,
                            gdouble           velocity);
};

GType           mech_acceleration_get_type     (void) G_GNUC_CONST;
MechAnimation * mech_acceleration_new          (gdouble           initial_position,
                                                gdouble           initial_velocity,
                                                gdouble           acceleration);
gdouble         mech_acceleration_get_boundary (MechAcceleration *acceleration);
void            mech_acceleration_set_boundary (MechAcceleration *acceleration,
                                                gdouble           boundary_pos);

G_END_DECLS

#endif /* __MECH_ACCELERATION_H__ */
