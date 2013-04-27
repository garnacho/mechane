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

#ifndef __MECH_CLOCK_PRIVATE_H__
#define __MECH_CLOCK_PRIVATE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_CLOCK         (_mech_clock_get_type ())
#define MECH_CLOCK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_CLOCK, MechClock))
#define MECH_CLOCK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_CLOCK, MechClockClass))
#define MECH_IS_CLOCK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_CLOCK))
#define MECH_IS_CLOCK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_CLOCK))
#define MECH_CLOCK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_CLOCK, MechClockClass))

typedef struct _MechClock MechClock;
typedef struct _MechClockClass MechClockClass;

struct _MechClock
{
  GObject parent_instance;
};

struct _MechClockClass
{
  GObjectClass parent_class;

  void     (* start)    (MechClock *clock);
  void     (* stop)     (MechClock *clock);
  gboolean (* dispatch) (MechClock *clock);
  gint64   (* get_time) (MechClock *clock);
};

GType       _mech_clock_get_type         (void) G_GNUC_CONST;

void        _mech_clock_tick             (MechClock     *clock);
gboolean    _mech_clock_dispatch         (MechClock     *clock);
gint64      _mech_clock_get_time         (MechClock     *clock);

G_END_DECLS

#endif /* __MECH_CLOCK_PRIVATE_H__ */
