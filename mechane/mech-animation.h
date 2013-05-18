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

#ifndef __MECH_ANIMATION_H__
#define __MECH_ANIMATION_H__

#include <mechane/mech-window.h>

G_BEGIN_DECLS

#define MECH_TYPE_ANIMATION         (mech_animation_get_type ())
#define MECH_ANIMATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_ANIMATION, MechAnimation))
#define MECH_ANIMATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_ANIMATION, MechAnimationClass))
#define MECH_IS_ANIMATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_ANIMATION))
#define MECH_IS_ANIMATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_ANIMATION))
#define MECH_ANIMATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_ANIMATION, MechAnimationClass))

typedef struct _MechAnimation MechAnimation;
typedef struct _MechAnimationClass MechAnimationClass;

struct _MechAnimation
{
  GObject parent_instance;
};

struct _MechAnimationClass
{
  GObjectClass parent_class;

  void     (* begin) (MechAnimation *animation,
                      gint64         _time);
  gboolean (* frame) (MechAnimation *animation,
                      gint64         _time);
  void     (* end)   (MechAnimation *animation,
                      gint64         _time);
};

GType    mech_animation_get_type   (void) G_GNUC_CONST;
void     mech_animation_run        (MechAnimation *animation,
                                    MechWindow    *window);
void     mech_animation_stop       (MechAnimation *animation);

gboolean mech_animation_is_running (MechAnimation *animation);


G_END_DECLS

#endif /* __MECH_ANIMATION_H__ */
