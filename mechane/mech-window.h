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

#include <mechane/mech-container.h>
#include <mechane/mech-monitor.h>
#include <mechane/mech-style.h>
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
  MechContainer parent_instance;
};

struct _MechWindowClass
{
  MechContainerClass parent_class;

  gboolean (* move)          (MechWindow        *window,
                              MechEvent         *event);
  gboolean (* resize)        (MechWindow        *window,
                              MechEvent         *event,
                              MechSideFlags      side);
  void     (* apply_state)   (MechWindow        *window,
                              MechWindowState    state,
                              MechMonitor       *monitor);
  void     (* set_title)     (MechWindow        *window,
                              const gchar       *title);
  void     (* set_visible)   (MechWindow        *window,
                              gboolean           visible);

  gboolean (* close_request) (MechWindow        *window);
};

GType             mech_window_get_type      (void) G_GNUC_CONST;

MechWindow      * mech_window_new           (void);

void              mech_window_set_resizable (MechWindow      *window,
                                             gboolean         resizable);
gboolean          mech_window_get_resizable (MechWindow      *window);

void              mech_window_set_visible   (MechWindow      *window,
                                             gboolean         visible);
gboolean          mech_window_get_visible   (MechWindow      *window);

void              mech_window_push_state    (MechWindow      *window,
                                             MechWindowState  state,
                                             MechMonitor     *monitor);
void              mech_window_pop_state     (MechWindow      *window);
MechWindowState   mech_window_get_state     (MechWindow      *window);

void              mech_window_set_title     (MechWindow      *window,
                                             const gchar     *title);
const gchar     * mech_window_get_title     (MechWindow      *window);

MechMonitor     * mech_window_get_monitor   (MechWindow      *window);

gboolean          mech_window_move          (MechWindow      *window,
                                             MechEvent       *event);
gboolean          mech_window_resize        (MechWindow      *window,
                                             MechEvent       *event,
                                             MechSideFlags    side);

void              mech_window_set_style     (MechWindow      *window,
                                             MechStyle       *style);
MechStyle       * mech_window_get_style     (MechWindow      *window);

gboolean          mech_window_set_renderer_type (MechWindow        *window,
                                                 MechRendererType   renderer_type,
                                                 GError           **error);
MechRendererType  mech_window_get_renderer_type (MechWindow        *window);

G_END_DECLS

#endif /* __MECH_WINDOW_H__ */
