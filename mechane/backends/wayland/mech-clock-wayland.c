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

#include "mech-clock-wayland.h"

G_DEFINE_TYPE (MechClockWayland, mech_clock_wayland, MECH_TYPE_CLOCK)

static gboolean _clock_ensure_frame_callback (MechClockWayland *clock,
                                              gboolean          overwrite);
static void     _clock_unset_frame_callback  (MechClockWayland *clock);

enum {
  PROP_WL_SURFACE = 1
};

struct _MechClockWaylandPriv
{
  struct wl_surface *wl_surface;
  struct wl_callback *wl_callback;
  gint64 cached_time;
  gint64 frame_count_time;
  guint frame_count;
  guint idle_id;
};

#undef FPS_DEBUGGING

static void
_print_and_reset_stats (MechClockWayland *clock,
                        gboolean          force)
{
#ifdef FPS_DEBUGGING
  MechClockWaylandPriv *priv = clock->_priv;
  gint64 diff = ABS (priv->frame_count_time - priv->cached_time);

  if (!force && diff < G_USEC_PER_SEC)
    return;

  if (priv->frame_count == 0 || priv->frame_count_time == 0)
    return;

  g_print ("Got %d frames in %ld milliseconds (%.3f FPS)\n",
           priv->frame_count, diff / 1000,
           ((gdouble) priv->frame_count * 1000000) / diff);
  priv->frame_count = 0;
  priv->frame_count_time = 0;
#endif /* FPS_DEBUGGING */
}

static void
_clock_frame_callback (gpointer            data,
                       struct wl_callback *wl_callback,
                       guint32             serial)
{
  MechClockWayland *clock = data;
  MechClockWaylandPriv *priv = clock->_priv;
  gboolean cont;

  priv->frame_count++;
  priv->cached_time = g_get_monotonic_time ();
  cont = _mech_clock_dispatch ((MechClock *) clock);

  if (cont)
    {
      _clock_ensure_frame_callback (clock, TRUE);

      if (priv->frame_count_time <= 0)
        priv->frame_count_time = priv->cached_time;
      else
        _print_and_reset_stats (clock, FALSE);
    }
  else
    _clock_unset_frame_callback (clock);
}

static const struct wl_callback_listener frame_callback_listener_funcs = {
  _clock_frame_callback
};

static gboolean
_clock_ensure_frame_callback (MechClockWayland *clock,
                              gboolean          overwrite)
{
  MechClockWaylandPriv *priv = clock->_priv;

  if (priv->wl_callback)
    {
      if (!overwrite)
        return FALSE;

      wl_callback_destroy (priv->wl_callback);
    }

  priv->wl_callback = wl_surface_frame (priv->wl_surface);
  wl_callback_add_listener (priv->wl_callback,
                            &frame_callback_listener_funcs, clock);
  wl_surface_commit (priv->wl_surface);
  return TRUE;
}

static void
_clock_unset_frame_callback (MechClockWayland *clock)
{
  MechClockWaylandPriv *priv = clock->_priv;

  if (priv->wl_callback)
    {
      _print_and_reset_stats (clock, TRUE);
      wl_callback_destroy (priv->wl_callback);
      priv->wl_callback = NULL;
    }
}

static void
mech_clock_wayland_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  MechClockWaylandPriv *priv = ((MechClockWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_SURFACE:
      priv->wl_surface = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_clock_wayland_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  MechClockWaylandPriv *priv = ((MechClockWayland *) object)->_priv;

  switch (prop_id)
    {
    case PROP_WL_SURFACE:
      g_value_set_pointer (value, priv->wl_surface);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mech_clock_wayland_finalize (GObject *object)
{
  MechClockWaylandPriv *priv = ((MechClockWayland *) object)->_priv;

  if (priv->wl_callback)
    wl_callback_destroy (priv->wl_callback);

  G_OBJECT_CLASS (mech_clock_wayland_parent_class)->finalize (object);
}

static gboolean
_mech_clock_wayland_run (gpointer user_data)
{
  MechClockWaylandPriv *priv;

  priv = ((MechClockWayland *) user_data)->_priv;
  priv->cached_time = g_get_monotonic_time ();
  priv->idle_id = 0;

  _mech_clock_dispatch (user_data);

  return FALSE;
}

static void
mech_clock_wayland_start (MechClock *clock)
{
  MechClockWayland *clock_wayland = (MechClockWayland *) clock;
  MechClockWaylandPriv *priv = clock_wayland->_priv;

  priv->cached_time = g_get_monotonic_time ();

  if (_clock_ensure_frame_callback (clock_wayland, FALSE))
    {
      /* The frame callback will ensure the clock is updated after
       * each surface redraw, but the first frame has to be induced
       * so this idle ensures such first invalidation happens.
       */
      /* FIXME: ensure idle runs on the wayland thread */
      if (!priv->idle_id)
        g_idle_add (_mech_clock_wayland_run, clock);
    }
}

static void
mech_clock_wayland_stop (MechClock *clock)
{
  MechClockWayland *clock_wayland = (MechClockWayland *) clock;
  MechClockWaylandPriv *priv = clock_wayland->_priv;

  _clock_unset_frame_callback (clock_wayland);

  if (priv->idle_id)
    {
      g_source_remove (priv->idle_id);
      priv->idle_id = 0;
    }
}

static gboolean
mech_clock_wayland_dispatch (MechClock *clock)
{
  gboolean retval;

  retval = MECH_CLOCK_CLASS (mech_clock_wayland_parent_class)->dispatch (clock);

  if (!retval)
    _clock_unset_frame_callback ((MechClockWayland *) clock);

  return retval;
}

static gint64
mech_clock_wayland_get_time (MechClock *clock)
{
  MechClockWaylandPriv *priv = ((MechClockWayland *) clock)->_priv;

  if (priv->wl_callback || priv->idle_id)
    return priv->cached_time;

  return g_get_monotonic_time ();
}

static void
mech_clock_wayland_class_init (MechClockWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MechClockClass *clock_class = MECH_CLOCK_CLASS (klass);

  object_class->set_property = mech_clock_wayland_set_property;
  object_class->get_property = mech_clock_wayland_get_property;
  object_class->finalize = mech_clock_wayland_finalize;

  clock_class->start = mech_clock_wayland_start;
  clock_class->stop = mech_clock_wayland_stop;
  clock_class->dispatch = mech_clock_wayland_dispatch;
  clock_class->get_time = mech_clock_wayland_get_time;

  g_object_class_install_property (object_class,
                                   PROP_WL_SURFACE,
                                   g_param_spec_pointer ("wl-surface",
                                                         "wl-surface",
                                                         "wl-surface",
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (MechClockWaylandPriv));
}

static void
mech_clock_wayland_init (MechClockWayland *clock)
{
  clock->_priv = G_TYPE_INSTANCE_GET_PRIVATE (clock,
                                              MECH_TYPE_CLOCK_WAYLAND,
                                              MechClockWaylandPriv);
}

MechClock *
_mech_clock_wayland_new (MechWindowWayland *window,
                         struct wl_surface *wl_surface)
{
  return g_object_new (MECH_TYPE_CLOCK_WAYLAND,
                       "wl-surface", wl_surface,
                       "window", window,
                       NULL);
}
