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

#include "mech-backend-private.h"
#include "mech-enum-types.h"
#include "mech-enums.h"
#include "mech-cursor.h"

typedef struct _MechCursorPrivate MechCursorPrivate;

enum {
  PROP_CURSOR_TYPE = 1
};

struct _MechCursorPrivate
{
  MechCursorType cursor_type;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechCursor, mech_cursor, G_TYPE_OBJECT)

static void
mech_cursor_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MechCursorPrivate *priv;

  priv = mech_cursor_get_instance_private ((MechCursor *) object);

  switch (prop_id)
    {
    case PROP_CURSOR_TYPE:
      g_value_set_enum (value, priv->cursor_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_cursor_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MechCursorPrivate *priv;

  priv = mech_cursor_get_instance_private ((MechCursor *) object);

  switch (prop_id)
    {
    case PROP_CURSOR_TYPE:
      priv->cursor_type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mech_cursor_class_init (MechCursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mech_cursor_get_property;
  object_class->set_property = mech_cursor_set_property;

  g_object_class_install_property (object_class,
                                   PROP_CURSOR_TYPE,
                                   g_param_spec_enum ("cursor-type",
                                                      "Cursor type",
                                                      "Cursor type",
                                                      MECH_TYPE_CURSOR_TYPE,
                                                      MECH_CURSOR_BLANK,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));
}

static void
mech_cursor_init (MechCursor *cursor)
{
}

MechCursor *
mech_cursor_lookup (MechCursorType type)
{
#define N_CURSORS MECH_CURSOR_DRAG_BOTTOM_RIGHT + 1

  static MechCursor *predefined_cursors[N_CURSORS] = { 0 };

  g_return_val_if_fail (type >= MECH_CURSOR_BLANK &&
                        type < N_CURSORS, NULL);

#undef N_CURSORS

  if (!predefined_cursors[type])
    {
      MechBackend *backend;

      backend = mech_backend_get ();
      predefined_cursors[type] = mech_backend_create_cursor (backend, type);
    }

  return predefined_cursors[type];
}

MechCursorType
mech_cursor_get_cursor_type (MechCursor *cursor)
{
  MechCursorPrivate *priv;

  g_return_val_if_fail (MECH_IS_CURSOR (cursor), MECH_CURSOR_BLANK);

  priv = mech_cursor_get_instance_private (cursor);
  return priv->cursor_type;
}
