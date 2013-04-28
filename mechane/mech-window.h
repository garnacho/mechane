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

#ifndef __MECH_WINDOW_H__
#define __MECH_WINDOW_H__

typedef struct _MechWindow MechWindow;

#include <mechane/mech-monitor.h>
#include <mechane/mech-area.h>
#include <mechane/mech-events.h>

G_BEGIN_DECLS

#define MECH_TYPE_WINDOW         (mech_window_get_type ())
#define MECH_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_WINDOW, MechWindow))
#define MECH_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_WINDOW, MechWindowClass))
#define MECH_IS_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_WINDOW))
#define MECH_IS_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_WINDOW))
#define MECH_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_WINDOW, MechWindowClass))

typedef struct _MechWindowClass MechWindowClass;

struct _MechWindow
{
  GObject parent_instance;
};

struct _MechWindowClass
{
  GObjectClass parent_class;

  gboolean (* handle_event)  (MechWindow        *window,
                              MechEvent         *event);

  void     (* set_title)     (MechWindow        *window,
                              const gchar       *title);
  void     (* set_visible)   (MechWindow        *window,
                              gboolean           visible);

  void     (* size_changed)  (MechWindow        *window,
                              gint               width,
                              gint               height);
};

GType             mech_window_get_type      (void) G_GNUC_CONST;

gboolean          mech_window_handle_event  (MechWindow      *window,
                                             MechEvent       *event);

void              mech_window_get_size      (MechWindow      *window,
                                             gint            *width,
                                             gint            *height);

void              mech_window_set_resizable (MechWindow      *window,
                                             gboolean         resizable);
gboolean          mech_window_get_resizable (MechWindow      *window);

void              mech_window_set_visible   (MechWindow      *window,
                                             gboolean         visible);
gboolean          mech_window_get_visible   (MechWindow      *window);

void              mech_window_set_title     (MechWindow      *window,
                                             const gchar     *title);
const gchar     * mech_window_get_title     (MechWindow      *window);

MechMonitor     * mech_window_get_monitor   (MechWindow      *window);

G_END_DECLS

#endif /* __MECH_WINDOW_H__ */
