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

#ifndef __MECH_TEXT_ATTRIBUTES_H__
#define __MECH_TEXT_ATTRIBUTES_H__

#include <pango/pango.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_TEXT_ATTRIBUTES         (mech_text_attributes_get_type ())
#define MECH_TEXT_ATTRIBUTES(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_TEXT_ATTRIBUTES, MechTextAttributes))
#define MECH_TEXT_ATTRIBUTES_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_TEXT_ATTRIBUTES, MechTextAttributesClass))
#define MECH_IS_TEXT_ATTRIBUTES(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_TEXT_ATTRIBUTES))
#define MECH_IS_TEXT_ATTRIBUTES_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_TEXT_ATTRIBUTES))
#define MECH_TEXT_ATTRIBUTES_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_TEXT_ATTRIBUTES, MechTextAttributesClass))

typedef enum _MechTextAttributeFields MechTextAttributeFields;
typedef struct _MechTextAttributes MechTextAttributes;
typedef struct _MechTextAttributesClass MechTextAttributesClass;

enum _MechTextAttributeFields {
  MECH_TEXT_ATTRIBUTE_FONT_DESCRIPTION = 1 << 0,
  MECH_TEXT_ATTRIBUTE_BACKGROUND       = 1 << 1,
  MECH_TEXT_ATTRIBUTE_FOREGROUND       = 1 << 2,
  MECH_TEXT_ATTRIBUTE_ALL_FIELDS       = (1 << 3) - 1
};

struct _MechTextAttributes
{
  GObject parent_instance;
};

struct _MechTextAttributesClass
{
  GObjectClass parent_class;
};

GType                   mech_text_attributes_get_type       (void) G_GNUC_CONST;
MechTextAttributes *    mech_text_attributes_new            (void);

void                    mech_text_attributes_combine        (MechTextAttributes       *attribute,
                                                             const MechTextAttributes *other);

MechTextAttributeFields mech_text_attributes_get_set_fields (MechTextAttributes       *attribute);
void                    mech_text_attributes_unset_fields   (MechTextAttributes       *attribute,
                                                             MechTextAttributeFields   fields);

G_END_DECLS

#endif /* __MECH_TEXT_ATTRIBUTES_H__ */
