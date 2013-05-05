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

#ifndef __MECH_STYLE_PRIVATE_H__
#define __MECH_STYLE_PRIVATE_H__

#include <mechane/mech-style.h>

G_BEGIN_DECLS

typedef struct _MechPropertyInfo MechPropertyInfo;

typedef enum {
  MECH_PROPERTY_MARGIN = 1,
  MECH_PROPERTY_MARGIN_UNIT,
  MECH_PROPERTY_PADDING,
  MECH_PROPERTY_PADDING_UNIT,
  MECH_PROPERTY_CORNER_RADIUS,
  MECH_PROPERTY_CORNER_RADIUS_UNIT,
  MECH_PROPERTY_FONT,
  MECH_PROPERTY_FONT_SIZE,
  MECH_PROPERTY_FONT_SIZE_UNIT,
  MECH_PROPERTY_BORDER,
  MECH_PROPERTY_BORDER_UNIT,
  MECH_PROPERTY_BORDER_PATTERN,
  MECH_PROPERTY_BACKGROUND_PATTERN,
  MECH_PROPERTY_FOREGROUND_PATTERN,
  MECH_PROPERTY_LAST
} MechStyleProperty;

/* Property information */
const MechPropertyInfo *
               _mech_style_property_info_lookup            (const MechPropertyInfo   *parent,
                                                            const gchar              *name);
gboolean       _mech_style_property_info_is_layered        (const MechPropertyInfo   *info);
gboolean       _mech_style_property_info_is_associative    (const MechPropertyInfo   *info,
                                                            GType                    *type,
                                                            gint                     *default_key);
gboolean       _mech_style_property_info_get_subproperties (const MechPropertyInfo   *info,
                                                            MechPropertyInfo       ***children,
                                                            guint                    *n_children);
GType          _mech_style_property_info_get_property_type (const MechPropertyInfo   *info);
MechStyleProperty
               _mech_style_property_info_get_id            (const MechPropertyInfo   *info);


/* Style setters */
void           _mech_style_push_path    (MechStyle      *style,
                                         const gchar    *element,
                                         MechStateFlags  state);
void           _mech_style_pop_path     (MechStyle      *style);

void           _mech_style_set_property (MechStyle      *style,
                                         guint           property,
                                         gint            layer,
                                         guint           flags,
                                         GValue         *value);

G_END_DECLS

#endif /* __MECH_STYLE_PRIVATE_H__ */
