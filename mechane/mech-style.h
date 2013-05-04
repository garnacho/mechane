/* Mechane:
 * Copyright (C) 2013 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __MECH_STYLE_H__
#define __MECH_STYLE_H__

typedef struct _MechStyle MechStyle;

#include <mechane/mech-area.h>
#include <mechane/mech-renderer.h>

G_BEGIN_DECLS

#define MECH_TYPE_STYLE         (mech_style_get_type ())
#define MECH_STYLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_STYLE, MechStyle))
#define MECH_STYLE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_STYLE, MechStyleClass))
#define MECH_IS_STYLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_STYLE))
#define MECH_IS_STYLE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_STYLE))
#define MECH_STYLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_STYLE, MechStyleClass))

typedef struct _MechStyleClass MechStyleClass;

struct _MechStyle
{
  GObject parent_instance;
};

struct _MechStyleClass
{
  GObjectClass parent_class;
};

GType          mech_style_get_type        (void) G_GNUC_CONST;

MechStyle    * mech_style_new             (void);

MechRenderer * mech_style_lookup_renderer (MechStyle      *style,
                                           MechArea       *area,
                                           MechStateFlags  state);

#endif /* __MECH_STYLE_H__ */
