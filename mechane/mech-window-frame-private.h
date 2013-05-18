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

#ifndef __MECH_WINDOW_FRAME_H__
#define __MECH_WINDOW_FRAME_H__

#include <mechane/mech-fixed-box.h>
#include <mechane/mech-events.h>

G_BEGIN_DECLS

#define MECH_TYPE_WINDOW_FRAME         (mech_window_frame_get_type ())
#define MECH_WINDOW_FRAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_WINDOW_FRAME, MechWindowFrame))
#define MECH_WINDOW_FRAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_WINDOW_FRAME, MechWindowFrameClass))
#define MECH_IS_WINDOW_FRAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_WINDOW_FRAME))
#define MECH_IS_WINDOW_FRAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_WINDOW_FRAME))
#define MECH_WINDOW_FRAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_WINDOW_FRAME, MechWindowFrameClass))

typedef struct _MechWindowFrame MechWindowFrame;
typedef struct _MechWindowFrameClass MechWindowFrameClass;

struct _MechWindowFrame
{
  MechFixedBox parent_instance;
};

struct _MechWindowFrameClass
{
  MechFixedBoxClass parent_class;

  void (* move)   (MechWindowFrame *frame,
                   MechEvent       *event);
  void (* resize) (MechWindowFrame *frame,
                   MechEvent       *event,
                   MechSideFlags    side);
  void (* close)  (MechWindowFrame *frame);
};

GType      mech_window_frame_get_type      (void) G_GNUC_CONST;

MechArea * mech_window_frame_new           (void);
void       mech_window_frame_set_maximized (MechWindowFrame *frame,
                                            gboolean         maximized);
gboolean   mech_window_frame_get_maximized (MechWindowFrame *frame);

void       mech_window_frame_set_resizable (MechWindowFrame *frame,
                                            gboolean         resizable);
gboolean   mech_window_frame_get_resizable (MechWindowFrame *frame);


G_END_DECLS

#endif /* __MECH_WINDOW_FRAME_H__ */
