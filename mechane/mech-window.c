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

#include <cairo/cairo-gobject.h>

#include <mechane/mech-marshal.h>
#include <mechane/mech-backend-private.h>
#include <mechane/mech-area-private.h>
#include <mechane/mech-window-frame-private.h>
#include <mechane/mech-clock-private.h>
#include <mechane/mech-window-private.h>
#include <mechane/mechane.h>

enum {
  CROSSING_ENTER = 1 << 1,
  CROSSING_LEAVE = 1 << 2,
};

typedef struct _MechWindowPrivate MechWindowPrivate;
typedef struct _MechStateData MechStateData;

struct _MechStateData
{
  MechMonitor *monitor;
  guint state;
  gint previous_width;
  gint previous_height;
};

struct _MechWindowPrivate
{
  MechMonitor *monitor;
  MechClock *clock;
  MechStyle *style;
  MechArea *frame;

  GArray *state;
  gchar *title;

  guint resizable     : 1;
  guint visible       : 1;
  guint renderer_type : 2;
};

enum {
  CLOSE_REQUEST,
  LAST_SIGNAL
};

enum {
  PROP_TITLE = 1,
  PROP_RESIZABLE,
  PROP_VISIBLE,
  PROP_MONITOR,
  PROP_STATE,
  PROP_STYLE,
  PROP_RENDERER_TYPE
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechWindow, mech_window,
                                     MECH_TYPE_CONTAINER)

static void
mech_window_get_property (GObject    *object,
                          guint       param_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MechWindow *window = (MechWindow *) object;
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);

  switch (param_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_RESIZABLE:
      g_value_set_boolean (value, priv->resizable);
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, priv->visible);
      break;
    case PROP_MONITOR:
      g_value_set_object (value, priv->monitor);
      break;
    case PROP_STATE:
      g_value_set_enum (value, mech_window_get_state (window));
      break;
    case PROP_STYLE:
      g_value_set_object (value, priv->style);
      break;
    case PROP_RENDERER_TYPE:
      g_value_set_enum (value, priv->renderer_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
mech_window_set_property (GObject      *object,
                          guint         param_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MechWindow *window = (MechWindow *) object;

  switch (param_id)
    {
    case PROP_TITLE:
      mech_window_set_title (window, g_value_get_string (value));
      break;
    case PROP_RESIZABLE:
      mech_window_set_resizable (window, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE:
      mech_window_set_visible (window, g_value_get_boolean (value));
      break;
    case PROP_STYLE:
      mech_window_set_style (window, g_value_get_object (value));
      break;
    case PROP_RENDERER_TYPE:
      mech_window_set_renderer_type (window, g_value_get_enum (value), NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
mech_window_constructed (GObject *object)
{
  G_OBJECT_CLASS (mech_window_parent_class)->constructed (object);
  mech_window_set_renderer_type ((MechWindow *) object,
                                 MECH_RENDERER_TYPE_SOFTWARE, NULL);
}

static void
mech_window_finalize (GObject *object)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private ((MechWindow *) object);

  g_free (priv->title);
  g_object_unref (priv->clock);

  if (priv->state)
    g_array_unref (priv->state);

  g_signal_handlers_disconnect_by_data (priv->frame, object);
  _mech_area_set_window (priv->frame, NULL);
  g_object_unref (priv->frame);
  g_object_unref (priv->style);

  G_OBJECT_CLASS (mech_window_parent_class)->finalize (object);
}

static void
mech_window_draw (MechContainer  *container,
                  cairo_t        *cr,
                  cairo_region_t *clip)
{
  MechWindow *window = (MechWindow *) container;
  MechWindowPrivate *priv;

  if (!mech_window_get_visible (window))
    return;

  MECH_CONTAINER_CLASS (mech_window_parent_class)->draw (container, cr, clip);
}

static void
mech_window_update_notify (MechContainer *container)
{
  MechWindow *window = (MechWindow *) container;
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);

  /* Make the clock tick to process the updates */
  if (priv->clock)
    _mech_clock_tick (priv->clock);
}

static void
window_frame_close (MechWindowFrame *frame,
                    MechWindow      *window)
{
  gboolean retval;

  g_object_ref (window);
  g_signal_emit (window, signals[CLOSE_REQUEST], 0, &retval);

  if (!retval)
    mech_window_set_visible (window, FALSE);

  g_object_unref (window);
}

static void
window_frame_maximize_toggle (MechWindowFrame *frame,
                              GParamSpec      *pspec,
                              MechWindow      *window)
{
  if (mech_window_frame_get_maximized (frame))
    mech_window_push_state (window, MECH_WINDOW_STATE_MAXIMIZED, NULL);
  else
    mech_window_pop_state (window);
}

static MechArea *
mech_window_create_root (MechContainer *container)
{
  MechWindow *window = (MechWindow *) container;
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  priv->frame = mech_window_frame_new ();

  _mech_area_set_window (priv->frame, window);

  g_signal_connect_swapped (priv->frame, "move",
                            G_CALLBACK (mech_window_move), window);
  g_signal_connect_swapped (priv->frame, "resize",
                            G_CALLBACK (mech_window_resize), window);
  g_signal_connect (priv->frame, "close",
                    G_CALLBACK (window_frame_close), window);
  g_signal_connect (priv->frame, "notify::maximized",
                    G_CALLBACK (window_frame_maximize_toggle), window);

  return g_object_ref (priv->frame);
}

static void
mech_window_class_init (MechWindowClass *klass)
{
  MechContainerClass *container_class = (MechContainerClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->get_property = mech_window_get_property;
  object_class->set_property = mech_window_set_property;
  object_class->constructed = mech_window_constructed;
  object_class->finalize = mech_window_finalize;

  container_class->draw = mech_window_draw;
  container_class->create_root = mech_window_create_root;
  container_class->update_notify = mech_window_update_notify;

  signals[CLOSE_REQUEST] =
    g_signal_new ("close-request",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechWindowClass, close_request),
                  g_signal_accumulator_true_handled, NULL,
                  _mech_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "window title",
                                                        "The window title",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_RESIZABLE,
                                   g_param_spec_boolean ("resizable",
                                                         "resizable",
                                                         "Whether the window can be resized by the user",
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_VISIBLE,
                                   g_param_spec_boolean ("visible",
                                                         "visible",
                                                         "Whether the window is visible",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_MONITOR,
                                   g_param_spec_object ("monitor",
                                                        "Monitor",
                                                        "Monitor displaying the window",
                                                        MECH_TYPE_MONITOR,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_STATE,
                                   g_param_spec_enum ("state",
                                                      "State",
                                                      "Window state",
                                                      MECH_TYPE_WINDOW_STATE,
                                                      MECH_WINDOW_STATE_NORMAL,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_STYLE,
                                   g_param_spec_object ("style",
                                                        "Style",
                                                        "Style used to render "
                                                        "the window contents",
                                                        MECH_TYPE_STYLE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_RENDERER_TYPE,
                                   g_param_spec_enum ("renderer-type",
                                                      "Renderer type",
                                                      "Type of renderer drawing window contents",
                                                      MECH_TYPE_RENDERER_TYPE,
                                                      MECH_RENDERER_TYPE_SOFTWARE,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
}

static MechStyle *
_load_default_style (void)
{
  GError *error = NULL;
  MechTheme *theme;
  MechStyle *style;
  GFile *file;

  style = mech_style_new ();
  theme = mech_theme_new ();

  file = g_file_new_for_uri ("resource://org/mechane/libmechane/DefaultTheme/style");
  mech_theme_load_style_from_file (theme, style, file, &error);
  g_object_unref (file);
  g_object_unref (theme);

  if (error)
    {
      g_warning ("Error loading default theme: %s\n", error->message);
      g_object_unref (style);
      g_error_free (error);
      return NULL;
    }

  return style;
}

static void
mech_window_init (MechWindow *window)
{
  MechWindowPrivate *priv;
  MechStyle *style;

  priv = mech_window_get_instance_private (window);
  priv->resizable = TRUE;

  style = _load_default_style ();

  if (style)
    {
      mech_window_set_style (window, style);
      g_object_unref (style);
    }
}

MechWindow *
mech_window_new (void)
{
  MechBackend *backend;

  backend = mech_backend_get ();
  g_assert (backend != NULL);

  return mech_backend_create_window (backend);
}

void
mech_window_set_resizable (MechWindow *window,
                           gboolean    resizable)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);
  priv->resizable = (resizable == TRUE);
  mech_window_frame_set_resizable (MECH_WINDOW_FRAME (priv->frame), resizable);
  g_object_notify ((GObject *) window, "resizable");
}

gboolean
mech_window_get_resizable (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), FALSE);

  priv = mech_window_get_instance_private (window);
  return priv->resizable;
}

void
mech_window_set_visible (MechWindow *window,
                         gboolean    visible)
{
  MechContainer *container;
  MechWindowPrivate *priv;
  gint width, height;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);
  container = (MechContainer *) window;

  if (!priv->visible && visible &&
      !mech_container_get_size (container, &width, &height))
    mech_container_queue_resize (container, width, height);

  if (MECH_WINDOW_GET_CLASS (window)->set_visible)
    MECH_WINDOW_GET_CLASS (window)->set_visible (window, visible);
  else
    g_warning ("%s: no vmethod implementation", G_STRFUNC);

  priv->visible = (visible == TRUE);
  g_object_notify ((GObject *) window, "visible");
}

gboolean
mech_window_get_visible (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), FALSE);

  priv = mech_window_get_instance_private (window);
  return priv->visible;
}

void
mech_window_push_state (MechWindow      *window,
                        MechWindowState  state,
                        MechMonitor     *monitor)
{
  MechWindowPrivate *priv;
  MechStateData data;

  g_return_if_fail (MECH_IS_WINDOW (window));
  g_return_if_fail (!monitor || MECH_IS_MONITOR (monitor));

  priv = mech_window_get_instance_private (window);

  if (!priv->state)
    priv->state = g_array_new (FALSE, FALSE, sizeof (MechStateData));

  data.state = state;
  data.monitor = monitor;
  mech_container_get_size ((MechContainer *) window,
                           &data.previous_width, &data.previous_height);
  g_array_append_val (priv->state, data);

  MECH_WINDOW_GET_CLASS (window)->apply_state (window, state, monitor);
}

void
mech_window_pop_state (MechWindow *window)
{
  MechWindowState state = MECH_WINDOW_STATE_NORMAL;
  gint previous_width, previous_height;
  MechMonitor *monitor = NULL;
  MechWindowPrivate *priv;
  MechStateData *data;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);

  if (!priv->state || priv->state->len == 0)
    return;

  data = &g_array_index (priv->state, MechStateData,
                         priv->state->len - 1);
  previous_width = data->previous_width;
  previous_height = data->previous_height;

  g_array_remove_index (priv->state, priv->state->len - 1);

  if (priv->state->len != 0)
    {
      data = &g_array_index (priv->state, MechStateData,
                             priv->state->len - 1);
      state = data->state;
      monitor = data->monitor;
    }

  MECH_WINDOW_GET_CLASS (window)->apply_state (window, state, monitor);

  if (state == MECH_WINDOW_STATE_NORMAL)
    mech_container_queue_resize ((MechContainer *) window,
                                 previous_width, previous_height);
}

MechWindowState
mech_window_get_state (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), MECH_WINDOW_STATE_NORMAL);

  priv = mech_window_get_instance_private (window);

  if (!priv->state || priv->state->len == 0)
    return MECH_WINDOW_STATE_NORMAL;

  return g_array_index (priv->state, MechStateData,
                        priv->state->len - 1).state;
}

void
mech_window_set_title (MechWindow  *window,
                       const gchar *title)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);
  g_free (priv->title);
  priv->title = g_strdup (title);

  if (!MECH_WINDOW_GET_CLASS (window)->set_title)
    {
      g_warning ("%s: no vmethod implementation", G_STRFUNC);
      return;
    }

  MECH_WINDOW_GET_CLASS (window)->set_title (window, priv->title);
  mech_text_set_string (MECH_TEXT (priv->frame), title);
}

const gchar *
mech_window_get_title (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), NULL);

  priv = mech_window_get_instance_private (window);
  return priv->title;
}

void
_mech_window_set_monitor (MechWindow  *window,
                          MechMonitor *monitor)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  priv->monitor = monitor;
}

MechMonitor *
mech_window_get_monitor (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), NULL);

  priv = mech_window_get_instance_private (window);
  return priv->monitor;
}

void
_mech_window_set_clock (MechWindow *window,
                        MechClock  *clock)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);

  if (priv->clock)
    {
      g_object_unref (priv->clock);
      priv->clock = NULL;
    }

  if (clock)
    {
      priv->clock = g_object_ref (clock);
      _mech_clock_tick (clock);
    }
}

MechClock *
_mech_window_get_clock (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  return priv->clock;
}

gboolean
mech_window_move (MechWindow *window,
                  MechEvent  *event)
{
  g_return_val_if_fail (MECH_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type != MECH_KEY_PRESS && event->type == MECH_KEY_PRESS &&
      event->type != MECH_BUTTON_PRESS && event->type == MECH_BUTTON_RELEASE)
    return FALSE;

  return MECH_WINDOW_GET_CLASS (window)->move (window, event);
}

gboolean
mech_window_resize (MechWindow    *window,
                    MechEvent     *event,
                    MechSideFlags  side)
{
  g_return_val_if_fail (MECH_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type != MECH_KEY_PRESS && event->type == MECH_KEY_PRESS &&
      event->type != MECH_BUTTON_PRESS && event->type == MECH_BUTTON_RELEASE)
    return FALSE;

  return MECH_WINDOW_GET_CLASS (window)->resize (window, event, side);
}

void
mech_window_set_style (MechWindow *window,
                       MechStyle  *style)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));
  g_return_if_fail (MECH_IS_STYLE (style));

  priv = mech_window_get_instance_private (window);

  if (style)
    g_object_ref (style);

  if (priv->style)
    g_object_unref (priv->style);

  priv->style = style;
  g_object_notify ((GObject *) window, "style");
}

MechStyle *
mech_window_get_style (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), NULL);

  priv = mech_window_get_instance_private (window);
  return priv->style;
}

gboolean
mech_window_set_renderer_type (MechWindow        *window,
                               MechRendererType   renderer_type,
                               GError           **error)
{
  MechSurfaceType surface_type;
  MechWindowPrivate *priv;
  gboolean retval = TRUE;
  MechSurface *surface;

  g_return_val_if_fail (MECH_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (surface_type != MECH_RENDERER_TYPE_SOFTWARE ||
                        surface_type != MECH_RENDERER_TYPE_GL, FALSE);
  g_return_val_if_fail (!error || !*error, FALSE);

  priv = mech_window_get_instance_private (window);

  if (priv->renderer_type == renderer_type &&
      _mech_container_get_surface ((MechContainer *) window))
    return TRUE;

  if (renderer_type == MECH_RENDERER_TYPE_SOFTWARE)
    surface_type = MECH_SURFACE_TYPE_SOFTWARE;
  else if (renderer_type == MECH_RENDERER_TYPE_GL)
    surface_type = MECH_SURFACE_TYPE_GL;
  else
    g_assert_not_reached ();

  surface = _mech_surface_new (NULL, NULL, surface_type);
  retval = g_initable_init (G_INITABLE (surface), NULL, error);

  if (!retval)
    {
      g_object_unref (surface);
      surface = _mech_surface_new (NULL, NULL, MECH_SURFACE_TYPE_SOFTWARE);

      /* This should really always succeed */
      if (!g_initable_init (G_INITABLE (surface), NULL, NULL))
        g_assert_not_reached ();
    }

  _mech_container_set_surface ((MechContainer *) window, surface);
  renderer_type = _mech_surface_get_renderer_type (surface);
  g_object_unref (surface);

  if (priv->renderer_type != renderer_type)
    {
      priv->renderer_type = renderer_type;
      g_object_notify ((GObject *) window, "renderer-type");
    }

  return retval;
}

MechRendererType
mech_window_get_renderer_type (MechWindow *window)
{
  MechWindowPrivate *priv;

  g_return_val_if_fail (MECH_IS_WINDOW (window), MECH_RENDERER_TYPE_SOFTWARE);

  priv = mech_window_get_instance_private (window);
  return priv->renderer_type;
}
