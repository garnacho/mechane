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

#ifndef __MECH_TEXT_H__
#define __MECH_TEXT_H__

#include <glib-object.h>
#include <mechane/mech-area.h>
#include <mechane/mech-text-buffer.h>

G_BEGIN_DECLS

#define MECH_TYPE_TEXT          (mech_text_get_type ())
#define MECH_TEXT(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_TEXT, MechText))
#define MECH_IS_TEXT(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_TEXT))
#define MECH_TEXT_GET_IFACE(o)  (G_TYPE_INSTANCE_GET_INTERFACE ((o), MECH_TYPE_TEXT, MechTextInterface))

typedef struct _MechText MechText;
typedef struct _MechTextInterface MechTextInterface;

struct _MechTextInterface
{
  GTypeInterface parent_iface;
};

GType            mech_text_get_type   (void) G_GNUC_CONST;

void             mech_text_set_buffer (MechText       *text,
                                       MechTextBuffer *buffer);
MechTextBuffer * mech_text_get_buffer (MechText       *text);

/* Simple helpers */
void             mech_text_set_string (MechText       *text,
                                       const gchar    *str);
gchar *          mech_text_get_string (MechText       *text);


G_END_DECLS

#endif /* __MECH_TEXT_H__ */
