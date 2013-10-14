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

#ifndef __MECH_CONTAINER_H__
#define __MECH_CONTAINER_H__

#include <mechane/mech-area.h>
#include <mechane/mech-events.h>

G_BEGIN_DECLS

#define MECH_TYPE_CONTAINER         (mech_container_get_type ())
#define MECH_CONTAINER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_CONTAINER, MechContainer))
#define MECH_CONTAINER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_CONTAINER, MechContainerClass))
#define MECH_IS_CONTAINER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_CONTAINER))
#define MECH_IS_CONTAINER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_CONTAINER))
#define MECH_CONTAINER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_CONTAINER, MechContainerClass))

typedef struct _MechContainer MechContainer;
typedef struct _MechContainerClass MechContainerClass;
typedef struct _MechContainerPriv MechContainerPriv;

struct _MechContainer
{
  GObject parent_instance;
};

struct _MechContainerClass
{
  GObjectClass parent_class;

  void       (* draw)            (MechContainer  *container,
                                  cairo_t        *cr);
  gboolean   (* handle_event)    (MechContainer  *container,
                                  MechEvent      *event);

  MechArea * (* create_root)     (MechContainer  *container);

  void       (* update_notify)   (MechContainer  *container);
  void       (* size_changed)    (MechContainer  *container,
                                  gint            width,
                                  gint            height);

  void       (* grab_focus)      (MechContainer  *container,
                                  MechArea       *area,
                                  MechSeat       *seat);
};

GType      mech_container_get_type        (void) G_GNUC_CONST;

gboolean   mech_container_handle_event    (MechContainer *container,
                                           MechEvent     *event);

void       mech_container_queue_redraw    (MechContainer *container);
void       mech_container_queue_resize    (MechContainer *container,
                                           gint           width,
                                           gint           height);
void       mech_container_process_updates (MechContainer *container);

gboolean   mech_container_get_size        (MechContainer *container,
                                           gint          *width,
                                           gint          *height);
MechArea * mech_container_get_root        (MechContainer *container);

MechArea * mech_container_get_focus       (MechContainer *container);
void       mech_container_grab_focus      (MechContainer *container,
                                           MechArea      *area,
                                           MechSeat      *seat);

G_END_DECLS

#endif /* __MECH_CONTAINER_H__ */
