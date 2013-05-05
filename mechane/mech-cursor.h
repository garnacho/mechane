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

#ifndef __MECH_CURSOR_H__
#define __MECH_CURSOR_H__

#include <mechane/mech-enums.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_CURSOR         (mech_cursor_get_type ())
#define MECH_CURSOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_CURSOR, MechCursor))
#define MECH_CURSOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_CURSOR, MechCursorClass))
#define MECH_IS_CURSOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_CURSOR))
#define MECH_IS_CURSOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_CURSOR))
#define MECH_CURSOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_CURSOR, MechCursorClass))

typedef struct _MechCursor MechCursor;
typedef struct _MechCursorClass MechCursorClass;

struct _MechCursor
{
  GObject parent_instance;
};

struct _MechCursorClass
{
  GObjectClass parent_class;
};

GType            mech_cursor_get_type        (void) G_GNUC_CONST;

MechCursorType   mech_cursor_get_cursor_type (MechCursor     *cursor);

G_END_DECLS

#endif /* __MECH_CURSOR_H__ */
