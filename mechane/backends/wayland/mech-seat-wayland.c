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

#include <mechane/mech-container-private.h>
#include <xkbcommon/xkbcommon.h>
#include "mech-cursor-wayland.h"
#include "mech-seat-wayland.h"

enum {
  PROP_WL_SEAT = 1
};

struct _MechSeatWaylandPriv
{
  struct wl_seat *wl_seat;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;
  struct wl_touch *wl_touch;

  MechWindow *pointer_window;
  gdouble pointer_x;
  gdouble pointer_y;

  MechCursorWayland *pointer_cursor;
  struct wl_surface *pointer_surface;
  guint32 enter_serial;

  struct xkb_context *xkb_context;
  struct xkb_state *xkb_state;
  MechWindow *keyboard_window;
  guint32 active_modifiers;
  guint32 latched_modifiers;
  guint32 locked_modifiers;
};

G_DEFINE_TYPE (MechSeatWayland, mech_seat_wayland, MECH_TYPE_SEAT)

static void
mech_seat_wayland_get_modifiers (MechSeat *seat,
                                 guint    *active,
                                 guint    *latched,
                                 guint    *locked)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) seat)->_priv;

  if (active)
    *active = priv->active_modifiers;
  if (latched)
    *latched = priv->latched_modifiers;
  if (locked)
    *locked = priv->locked_modifiers;
}

static void
mech_seat_wayland_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_SEAT:
      priv->wl_seat = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_seat_wayland_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_SEAT:
      g_value_set_pointer (value, priv->wl_seat);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/* Pointer interface */
static void
mech_seat_wayland_check_cursor (MechSeatWayland *seat)
{
  MechSeatWaylandPriv *priv = seat->_priv;
  gint width, height, hotspot_x, hotspot_y;
  struct wl_buffer *wl_buffer;
  MechCursorWayland *cursor;

  priv = ((MechSeatWayland *) seat)->_priv;
  cursor = (MechCursorWayland *)
    _mech_container_get_current_cursor ((MechContainer *) priv->pointer_window);

  if (!cursor)
    {
      cursor = (MechCursorWayland *) mech_cursor_lookup (MECH_CURSOR_NORMAL);
      g_return_if_fail (cursor != NULL);
    }

  if (cursor == priv->pointer_cursor)
    return;

  priv->pointer_cursor = cursor;
  wl_buffer = mech_cursor_wayland_get_buffer (cursor, &width, &height,
                                              &hotspot_x, &hotspot_y);

  if (wl_buffer)
    {
      if (!priv->pointer_surface)
        {
          MechBackendWayland *backend;

          backend = _mech_backend_wayland_get ();
          priv->pointer_surface =
            wl_compositor_create_surface (backend->wl_compositor);
        }

      wl_pointer_set_cursor (priv->wl_pointer, priv->enter_serial,
                             priv->pointer_surface, hotspot_x, hotspot_y);

      wl_surface_attach (priv->pointer_surface, wl_buffer, 0, 0);
      wl_surface_damage (priv->pointer_surface, 0, 0, width, height);
      wl_surface_commit (priv->pointer_surface);
    }
  else
    wl_pointer_set_cursor (priv->wl_pointer, priv->enter_serial, NULL, 0, 0);
}

static void
_seat_pointer_enter (gpointer           data,
                     struct wl_pointer *wl_pointer,
                     guint32            serial,
                     struct wl_surface *wl_surface,
                     wl_fixed_t         surface_x,
                     wl_fixed_t         surface_y)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechBackendWayland *backend;
  MechEvent event = { 0 };

  backend = _mech_backend_wayland_get ();
  priv->enter_serial = serial;

  priv->pointer_window =
    _mech_backend_wayland_lookup_window (backend, wl_surface);
  priv->pointer_x = wl_fixed_to_double (surface_x);
  priv->pointer_x = wl_fixed_to_double (surface_y);

  event.type = MECH_ENTER;
  event.any.seat = data;
  event.any.serial = serial;
  mech_container_handle_event ((MechContainer *) priv->pointer_window, &event);

  mech_seat_wayland_check_cursor (data);
}

static void
_seat_pointer_leave (gpointer           data,
                     struct wl_pointer *wl_pointer,
                     guint32            serial,
                     struct wl_surface *wl_surface)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechEvent event = { 0 };

  event.type = MECH_LEAVE;
  event.any.seat = data;
  event.any.serial = serial;
  mech_container_handle_event ((MechContainer *) priv->pointer_window, &event);

  priv->pointer_window = NULL;
  priv->pointer_x = priv->pointer_y = -1;
  priv->enter_serial = 0;
  priv->pointer_cursor = NULL;
}

static void
_seat_pointer_motion (gpointer           data,
                      struct wl_pointer *wl_pointer,
                      guint32            time,
                      wl_fixed_t         surface_x,
                      wl_fixed_t         surface_y)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechEvent event = { 0 };
  MechSeat *seat = data;

  if (!priv->pointer_window)
    return;

  event.type = MECH_MOTION;
  event.any.seat = seat;
  event.input.evtime = time;
  event.input.modifiers =
    (priv->active_modifiers | priv->latched_modifiers | priv->locked_modifiers);
  event.pointer.x = priv->pointer_x = wl_fixed_to_double (surface_x);
  event.pointer.y = priv->pointer_y = wl_fixed_to_double (surface_y);

  mech_container_handle_event ((MechContainer *) priv->pointer_window, &event);

  mech_seat_wayland_check_cursor (data);
}

static void
_seat_pointer_button (gpointer           data,
                      struct wl_pointer *wl_pointer,
                      guint32            serial,
                      guint32            time,
                      guint32            button,
                      guint32            state)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechEvent event = { 0 };
  MechSeat *seat = data;

  if (!priv->pointer_window)
    return;

  event.type =
    (state == WL_POINTER_BUTTON_STATE_PRESSED) ?
    MECH_BUTTON_PRESS : MECH_BUTTON_RELEASE;
  event.any.seat = seat;
  event.any.serial = serial;
  event.input.evtime = time;
  event.input.modifiers =
    (priv->active_modifiers | priv->latched_modifiers | priv->locked_modifiers);
  event.pointer.x = priv->pointer_x;
  event.pointer.y = priv->pointer_y;

  mech_container_handle_event ((MechContainer *) priv->pointer_window, &event);
  mech_seat_wayland_check_cursor (data);
}

static void
_seat_pointer_axis (gpointer           data,
                    struct wl_pointer *wl_pointer,
                    guint32            time,
                    guint32            axis,
                    wl_fixed_t         value)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechEvent event = { 0 };
  MechSeat *seat = data;

  if (!priv->pointer_window)
    return;

  event.type = MECH_SCROLL;
  event.any.seat = seat;
  event.input.evtime = time;
  event.input.modifiers =
    (priv->active_modifiers | priv->latched_modifiers | priv->locked_modifiers);
  event.pointer.x = priv->pointer_x;
  event.pointer.y = priv->pointer_y;

  if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
    event.scroll.dx = wl_fixed_to_double (value);
  else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
    event.scroll.dy = wl_fixed_to_double (value);

  mech_container_handle_event ((MechContainer *) priv->pointer_window, &event);
  mech_seat_wayland_check_cursor (data);
}

static struct wl_pointer_listener pointer_listener_funcs = {
  _seat_pointer_enter,
  _seat_pointer_leave,
  _seat_pointer_motion,
  _seat_pointer_button,
  _seat_pointer_axis
};

static void
_mech_seat_wayland_set_pointer (MechSeatWayland   *seat,
                                struct wl_pointer *wl_pointer)
{
  if (seat->_priv->wl_pointer)
    wl_pointer_destroy(seat->_priv->wl_pointer);

  seat->_priv->wl_pointer = wl_pointer;

  if (wl_pointer)
    wl_pointer_add_listener (wl_pointer, &pointer_listener_funcs, seat);
}

/* Keyboard interface */
static void
_seat_keyboard_keymap (gpointer            data,
                       struct wl_keyboard *wl_keyboard,
                       guint32             format,
                       gint32              fd,
                       guint32             size)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  struct xkb_keymap *keymap;
  GMappedFile *mapped_file;
  GError *error = NULL;

  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    {
      close (fd);
      return;
    }

  mapped_file = g_mapped_file_new_from_fd (fd, FALSE, &error);

  if (error)
    {
      g_critical ("Could not load XKB keymap file: %s",
                  error->message);
      g_error_free (error);
      close (fd);
      return;
    }

  keymap = xkb_map_new_from_string (priv->xkb_context,
                                    g_mapped_file_get_contents (mapped_file),
                                    XKB_KEYMAP_FORMAT_TEXT_V1,
                                    0);
  g_mapped_file_unref (mapped_file);
  close (fd);

  if (!keymap)
    {
      g_critical ("Could not compile keymap");
      return;
    }

  if (priv->xkb_state)
    xkb_state_unref (priv->xkb_state);

  priv->xkb_state = xkb_state_new (keymap);
  xkb_map_unref (keymap);

  if (!priv->xkb_state)
    g_critical ("Could not create XKB state");
}

static void
_seat_keyboard_enter (gpointer            data,
                      struct wl_keyboard *wl_keyboard,
                      guint32             serial,
                      struct wl_surface  *wl_surface,
                      struct wl_array    *keys)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechBackendWayland *backend;
  MechEvent event = { 0 };

  backend = _mech_backend_wayland_get ();
  priv->keyboard_window = _mech_backend_wayland_lookup_window (backend,
                                                               wl_surface);
  event.type = MECH_FOCUS_IN;
  event.any.seat = data;
  event.any.serial = serial;
  mech_container_handle_event ((MechContainer *) priv->keyboard_window, &event);
}

static void
_seat_keyboard_leave (gpointer            data,
                      struct wl_keyboard *wl_keyboard,
                      guint32             serial,
                      struct wl_surface  *wl_surface)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechEvent event = { 0 };

  event.type = MECH_FOCUS_OUT;
  event.any.seat = data;
  event.any.serial = serial;
  mech_container_handle_event ((MechContainer *) priv->keyboard_window, &event);

  priv->keyboard_window = NULL;
}

static void
_seat_keyboard_key (gpointer            data,
                    struct wl_keyboard *wl_keyboard,
                    guint32             serial,
                    guint32             time,
                    guint32             key,
                    guint32             state)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  const xkb_keysym_t *keysyms;
  MechEvent event = { 0 };
  gchar buffer[7] = { 0 };

  if (!priv->xkb_state)
    return;

  event.type = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ?
    MECH_KEY_PRESS : MECH_KEY_RELEASE;
  event.any.seat = data;
  event.any.serial = serial;
  event.input.evtime = time;
  event.input.modifiers =
    (priv->active_modifiers | priv->latched_modifiers | priv->locked_modifiers);
  event.key.keycode = key;

  if (xkb_key_get_syms (priv->xkb_state, key + 8, &keysyms) > 0)
    event.key.keyval = keysyms[0];
  else
    event.key.keyval = XKB_KEY_NoSymbol;

  if (xkb_keysym_to_utf8 (event.key.keyval, buffer, sizeof (buffer)) > 0)
    event.key.unicode_char = g_utf8_get_char (buffer);

  mech_container_handle_event ((MechContainer *) priv->keyboard_window, &event);
}

static void
_seat_keyboard_modifiers (gpointer            data,
                          struct wl_keyboard *wl_keyboard,
                          guint32             serial,
                          guint32             mods_depressed,
                          guint32             mods_latched,
                          guint32             mods_locked,
                          guint32             group)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;

  priv->active_modifiers = mods_depressed;
  priv->latched_modifiers = mods_latched;
  priv->locked_modifiers = mods_locked;

  if (priv->xkb_state)
    xkb_state_update_mask (priv->xkb_state, mods_depressed, mods_latched,
                           mods_locked, 0, 0, group);
}

static struct wl_keyboard_listener keyboard_listener_funcs = {
  _seat_keyboard_keymap,
  _seat_keyboard_enter,
  _seat_keyboard_leave,
  _seat_keyboard_key,
  _seat_keyboard_modifiers
};

static void
_mech_seat_wayland_set_keyboard (MechSeatWayland    *seat,
                                 struct wl_keyboard *wl_keyboard)
{
  if (seat->_priv->wl_keyboard)
    wl_keyboard_destroy (seat->_priv->wl_keyboard);

  seat->_priv->wl_keyboard = wl_keyboard;

  if (wl_keyboard)
    wl_keyboard_add_listener (wl_keyboard, &keyboard_listener_funcs, seat);
}

static void
_seat_touch_down (gpointer           data,
                  struct wl_touch   *wl_touch,
                  guint32            serial,
                  guint32            time,
                  struct wl_surface *surface,
                  gint32             id,
                  wl_fixed_t         surface_x,
                  wl_fixed_t         surface_y)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechEvent event = { 0 };
  MechSeat *seat = data;

  if (!priv->pointer_window)
    return;

  event.type = MECH_TOUCH_DOWN;
  event.any.seat = seat;
  event.any.serial = serial;
  event.input.evtime = time;
  event.input.modifiers =
    (priv->active_modifiers | priv->latched_modifiers | priv->locked_modifiers);
  event.pointer.x = wl_fixed_to_double (surface_x);
  event.pointer.y = wl_fixed_to_double (surface_y);
  event.touch.id = id;

  mech_container_handle_event ((MechContainer *) priv->pointer_window, &event);
}

static void
_seat_touch_up (gpointer         data,
                struct wl_touch *wl_touch,
                guint32          serial,
                guint32          time,
                gint32           id)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechEvent event = { 0 };
  MechSeat *seat = data;

  if (!priv->pointer_window)
    return;

  event.type = MECH_TOUCH_UP;
  event.any.seat = seat;
  event.any.serial = serial;
  event.input.evtime = time;
  event.input.modifiers =
    (priv->active_modifiers | priv->latched_modifiers | priv->locked_modifiers);
  event.touch.id = id;

  /* FIXME: coordinates here are handy */

  mech_container_handle_event ((MechContainer *) priv->pointer_window, &event);
}

static void
_seat_touch_motion (gpointer         data,
                    struct wl_touch *wl_touch,
                    guint32          time,
                    gint32           id,
                    wl_fixed_t       surface_x,
                    wl_fixed_t       surface_y)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) data)->_priv;
  MechEvent event = { 0 };
  MechSeat *seat = data;

  if (!priv->pointer_window)
    return;

  event.type = MECH_TOUCH_MOTION;
  event.any.seat = seat;
  event.input.evtime = time;
  event.input.modifiers =
    (priv->active_modifiers | priv->latched_modifiers | priv->locked_modifiers);
  event.pointer.x = wl_fixed_to_double (surface_x);
  event.pointer.y = wl_fixed_to_double (surface_y);
  event.touch.id = id;

  mech_container_handle_event ((MechContainer *) priv->pointer_window, &event);
}

static void
_seat_touch_frame (gpointer         data,
                   struct wl_touch *wl_touch)
{
}

static void
_seat_touch_cancel (gpointer         data,
                    struct wl_touch *wl_touch)
{
  /* FIXME: cancel ongoing touch implicit grabs on the surface window */
}

static struct wl_touch_listener touch_listener_funcs = {
  _seat_touch_down,
  _seat_touch_up,
  _seat_touch_motion,
  _seat_touch_frame,
  _seat_touch_cancel
};

static void
_mech_seat_wayland_set_touch (MechSeatWayland *seat,
                              struct wl_touch *wl_touch)
{
  if (seat->_priv->wl_touch)
    wl_touch_destroy (seat->_priv->wl_touch);

  seat->_priv->wl_touch = wl_touch;

  if (wl_touch)
    wl_touch_add_listener (wl_touch, &touch_listener_funcs, seat);
}

void
_seat_capabilities_listener (gpointer        data,
                             struct wl_seat *wl_seat,
                             guint32         capabilities)
{
  MechSeatWayland *seat = data;
  MechSeatWaylandPriv *priv = seat->_priv;

  _mech_seat_wayland_set_pointer (seat, wl_seat_get_pointer (priv->wl_seat));
  _mech_seat_wayland_set_keyboard (seat, wl_seat_get_keyboard (priv->wl_seat));
  _mech_seat_wayland_set_touch (seat, wl_seat_get_touch (priv->wl_seat));
}

struct wl_seat_listener seat_listener_funcs = {
  _seat_capabilities_listener
};

static void
mech_seat_wayland_constructed (GObject *object)
{
  MechSeatWayland *seat = (MechSeatWayland *) object;
  MechSeatWaylandPriv *priv = seat->_priv;

  wl_seat_add_listener (priv->wl_seat, &seat_listener_funcs, object);
}

static void
mech_seat_wayland_finalize (GObject *object)
{
  MechSeatWaylandPriv *priv = ((MechSeatWayland *) object)->_priv;

  if (priv->wl_pointer)
    wl_pointer_destroy (priv->wl_pointer);
  if (priv->wl_keyboard)
    wl_keyboard_destroy (priv->wl_keyboard);
  if (priv->wl_touch)
    wl_touch_destroy (priv->wl_touch);
  if (priv->wl_seat)
    wl_seat_destroy (priv->wl_seat);
  if (priv->pointer_surface)
    wl_surface_destroy (priv->pointer_surface);
  if (priv->xkb_state)
    xkb_state_unref (priv->xkb_state);

  xkb_context_unref (priv->xkb_context);

  G_OBJECT_CLASS (mech_seat_wayland_parent_class)->finalize (object);
}

static void
mech_seat_wayland_class_init (MechSeatWaylandClass *klass)
{
  MechSeatClass *seat_class = (MechSeatClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  seat_class->get_modifiers = mech_seat_wayland_get_modifiers;

  object_class->set_property = mech_seat_wayland_set_property;
  object_class->get_property = mech_seat_wayland_get_property;
  object_class->constructed = mech_seat_wayland_constructed;
  object_class->finalize = mech_seat_wayland_finalize;

  g_object_class_install_property (object_class,
                                   PROP_WL_SEAT,
                                   g_param_spec_pointer ("wl-seat",
                                                         "wl-seat",
                                                         "wl-seat",
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (MechSeatWaylandPriv));
}

static void
mech_seat_wayland_init (MechSeatWayland *seat)
{
  seat->_priv = G_TYPE_INSTANCE_GET_PRIVATE (seat,
                                             MECH_TYPE_SEAT_WAYLAND,
                                             MechSeatWaylandPriv);
  seat->_priv->xkb_context = xkb_context_new (0);
}

MechSeat *
mech_seat_wayland_new (struct wl_seat *wl_seat)
{
  g_return_val_if_fail (wl_seat != NULL, NULL);

  return g_object_new (MECH_TYPE_SEAT_WAYLAND,
                       "wl-seat", wl_seat,
                       NULL);
}
