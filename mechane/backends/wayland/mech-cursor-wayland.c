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

#include "mech-cursor-wayland.h"
#include "mech-backend-wayland.h"
#include <wayland-cursor.h>

/* array matches MechCursorType */
gchar *cursor_names[] = {
  NULL,                  /* blank */
  "left_ptr",            /* normal */
  "xterm",               /* text edit */
  "watch",               /* busy */
  "grabbing",            /* dragging */
  "left_side",           /* drag left */
  "right_side",          /* drag right */
  "top_side",            /* drag top */
  "bottom_side",         /* drag bottom */
  "top_left_corner",     /* drag top left */
  "top_right_corner",    /* drag top right */
  "bottom_left_corner",  /* drag bottom left */
  "bottom_right_corner"  /* drag bottom right */
};

struct _MechCursorWaylandPriv
{
  struct wl_cursor *wl_cursor;
};

G_DEFINE_TYPE (MechCursorWayland, mech_cursor_wayland, MECH_TYPE_CURSOR)

static void
mech_cursor_wayland_constructed (GObject *object)
{
  MechCursor *cursor = (MechCursor *) object;
  MechCursorWaylandPriv *priv;
  MechCursorType cursor_type;

  priv = ((MechCursorWayland *) object)->_priv;
  cursor_type = mech_cursor_get_cursor_type (cursor);

  if (cursor_type > G_N_ELEMENTS (cursor_names))
    g_warning ("Invalid cursor type %d", cursor_type);
  else if (cursor_type != MECH_CURSOR_BLANK)
    {
      MechBackendWayland *backend;

      backend = _mech_backend_wayland_get ();
      priv->wl_cursor = wl_cursor_theme_get_cursor (backend->wl_cursor_theme,
                                                    cursor_names[cursor_type]);
      if (!priv->wl_cursor)
        g_warning ("Failed to load cursor '%s'", cursor_names[cursor_type]);
    }
}

static void
mech_cursor_wayland_class_init (MechCursorWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = mech_cursor_wayland_constructed;

  g_type_class_add_private (klass, sizeof (MechCursorWaylandPriv));
}

static void
mech_cursor_wayland_init (MechCursorWayland *cursor)
{
  cursor->_priv = G_TYPE_INSTANCE_GET_PRIVATE (cursor,
                                               MECH_TYPE_CURSOR_WAYLAND,
                                               MechCursorWaylandPriv);
}

struct wl_buffer *
mech_cursor_wayland_get_buffer (MechCursorWayland *cursor,
                                gint              *width,
                                gint              *height,
                                gint              *hotspot_x,
                                gint              *hotspot_y)
{
  struct wl_cursor_image *image;

  if (!cursor->_priv->wl_cursor)
    {
      *width = *height = *hotspot_x = *hotspot_y = 0;
      return NULL;
    }

  image = cursor->_priv->wl_cursor->images[0];

  *width = image->width;
  *height = image->height;
  *hotspot_x = image->hotspot_x;
  *hotspot_y = image->hotspot_y;

  return wl_cursor_image_get_buffer (image);
}
