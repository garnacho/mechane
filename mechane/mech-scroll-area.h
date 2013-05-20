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

#ifndef __MECH_SCROLL_AREA_H__
#define __MECH_SCROLL_AREA_H__

#include <mechane/mech-scroll-box.h>

G_BEGIN_DECLS

#define MECH_TYPE_SCROLL_AREA         (mech_scroll_area_get_type ())
#define MECH_SCROLL_AREA(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SCROLL_AREA, MechScrollArea))
#define MECH_SCROLL_AREA_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_SCROLL_AREA, MechScrollAreaClass))
#define MECH_IS_SCROLL_AREA(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SCROLL_AREA))
#define MECH_IS_SCROLL_AREA_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_SCROLL_AREA))
#define MECH_SCROLL_AREA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_SCROLL_AREA, MechScrollAreaClass))

typedef struct _MechScrollArea MechScrollArea;
typedef struct _MechScrollAreaClass MechScrollAreaClass;

struct _MechScrollArea
{
  MechScrollBox parent_instance;
};

struct _MechScrollAreaClass
{
  MechScrollBoxClass parent_class;
};

GType            mech_scroll_area_get_type (void) G_GNUC_CONST;
MechArea *       mech_scroll_area_new      (MechAxisFlags scrollbars);

void             mech_scroll_area_set_scrollbars (MechScrollArea *area,
                                                  MechAxisFlags   scrollbars);
MechAxisFlags    mech_scroll_area_get_scrollbars (MechScrollArea *area);

G_END_DECLS

#endif /* __MECH_SCROLL_AREA_H__ */
