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

#include <mechane/mech-marshal.h>
#include <mechane/mech-stage-private.h>
#include <mechane/mech-area-private.h>
#include <mechane/mech-window-private.h>

enum {
  CROSSING_ENTER = 1 << 1,
  CROSSING_LEAVE = 1 << 2,
};

typedef struct _MechWindowPrivate MechWindowPrivate;

struct _MechWindowPrivate
{
  MechStage *stage;
  MechClock *clock;

  gchar *title;

  gint width;
  gint height;

  guint resizable        : 1;
  guint visible          : 1;
};

enum {
  SIZE_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_TITLE = 1,
  PROP_RESIZABLE,
  PROP_VISIBLE
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MechWindow, mech_window, G_TYPE_OBJECT)

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
mech_window_finalize (GObject *object)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private ((MechWindow *) object);

  g_free (priv->title);
  g_object_unref (priv->stage);

  G_OBJECT_CLASS (mech_window_parent_class)->finalize (object);
}

static void
mech_window_class_init (MechWindowClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->get_property = mech_window_get_property;
  object_class->set_property = mech_window_set_property;
  object_class->finalize = mech_window_finalize;

  signals[SIZE_CHANGED] =
    g_signal_new ("size-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MechWindowClass, size_changed),
                  NULL, NULL,
                  _mech_marshal_VOID__INT_INT,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

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
}

static void
mech_window_init (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  priv->resizable = TRUE;
  priv->stage = _mech_stage_new ();
}

void
mech_window_set_resizable (MechWindow *window,
                           gboolean    resizable)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);
  priv->resizable = (resizable == TRUE);
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
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);

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
mech_window_get_size (MechWindow *window,
                      gint       *width,
                      gint       *height)
{
  MechWindowPrivate *priv;

  g_return_if_fail (MECH_IS_WINDOW (window));

  priv = mech_window_get_instance_private (window);

  _mech_stage_get_size (priv->stage, width, height);
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
    }
}

MechClock *
_mech_window_get_clock (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  return priv->clock;
}

MechStage *
_mech_window_get_stage (MechWindow *window)
{
  MechWindowPrivate *priv;

  priv = mech_window_get_instance_private (window);
  return priv->stage;
}
