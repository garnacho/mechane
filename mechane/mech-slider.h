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

#ifndef __MECH_SLIDER_H__
#define __MECH_SLIDER_H__

#include <mechane/mech-area.h>

G_BEGIN_DECLS

#define MECH_TYPE_SLIDER         (mech_slider_get_type ())
#define MECH_SLIDER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SLIDER, MechSlider))
#define MECH_SLIDER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_SLIDER, MechSliderClass))
#define MECH_IS_SLIDER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SLIDER))
#define MECH_IS_SLIDER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_SLIDER))
#define MECH_SLIDER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_SLIDER, MechSliderClass))

typedef struct _MechSlider MechSlider;
typedef struct _MechSliderClass MechSliderClass;

struct _MechSlider
{
  MechArea parent_instance;
};

struct _MechSliderClass
{
  MechAreaClass parent_class;
};

GType            mech_slider_get_type        (void) G_GNUC_CONST;
MechArea *       mech_slider_new             (MechOrientation  orientation);
MechArea *       mech_slider_new_with_handle (MechOrientation  orientation,
                                              MechArea        *area);

G_END_DECLS

#endif /* __MECH_SLIDER_H__ */
