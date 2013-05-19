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

#ifndef __MECH_ADJUSTABLE_H__
#define __MECH_ADJUSTABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_ADJUSTABLE          (mech_adjustable_get_type ())
#define MECH_ADJUSTABLE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_ADJUSTABLE, MechAdjustable))
#define MECH_IS_ADJUSTABLE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_ADJUSTABLE))
#define MECH_ADJUSTABLE_GET_IFACE(o)  (G_TYPE_INSTANCE_GET_INTERFACE ((o), MECH_TYPE_ADJUSTABLE, MechAdjustableInterface))

typedef struct _MechAdjustable MechAdjustable;
typedef struct _MechAdjustableInterface MechAdjustableInterface;

struct _MechAdjustableInterface
{
  GTypeInterface parent_iface;

  void (* value_changed) (MechAdjustable *adjustable);
};

GType      mech_adjustable_get_type           (void) G_GNUC_CONST;

void       mech_adjustable_get_bounds         (MechAdjustable *adjustable,
                                               gdouble        *min,
                                               gdouble        *max);
void       mech_adjustable_set_bounds         (MechAdjustable *adjustable,
                                               gdouble         min,
                                               gdouble         max);
gdouble    mech_adjustable_get_value          (MechAdjustable *adjustable);
void       mech_adjustable_set_value          (MechAdjustable *adjustable,
                                               gdouble         value);

gdouble    mech_adjustable_get_selection_size (MechAdjustable *adjustable);
void       mech_adjustable_set_selection_size (MechAdjustable *adjustable,
                                               gdouble         size);

G_END_DECLS

#endif /* __MECH_ADJUSTABLE_H__ */
