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

#ifndef __MECH_SEAT_H__
#define __MECH_SEAT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_SEAT         (mech_seat_get_type ())
#define MECH_SEAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SEAT, MechSeat))
#define MECH_SEAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_SEAT, MechSeatClass))
#define MECH_IS_SEAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SEAT))
#define MECH_IS_SEAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_SEAT))
#define MECH_SEAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_SEAT, MechSeatClass))

typedef struct _MechSeat MechSeat;
typedef struct _MechSeatClass MechSeatClass;

struct _MechSeat
{
  GObject parent_instance;
};

struct _MechSeatClass
{
  GObjectClass parent_class;

  void (* get_modifiers) (MechSeat *seat,
                          guint    *active,
                          guint    *latched,
                          guint    *locked);
};

GType   mech_seat_get_type       (void) G_GNUC_CONST;

guint   mech_seat_get_modifiers  (MechSeat *seat,
                                  guint    *active,
                                  guint    *latched,
                                  guint    *locked);

G_END_DECLS

#endif /* __MECH_SEAT_H__ */
