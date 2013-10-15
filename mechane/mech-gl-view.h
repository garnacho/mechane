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

#ifndef __MECH_GL_VIEW_H__
#define __MECH_GL_VIEW_H__

#include <mechane/mech-area.h>

G_BEGIN_DECLS

#define MECH_TYPE_GL_VIEW         (mech_gl_view_get_type ())
#define MECH_GL_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_GL_VIEW, MechGLView))
#define MECH_GL_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_GL_VIEW, MechGLViewClass))
#define MECH_IS_GL_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_GL_VIEW))
#define MECH_IS_GL_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_GL_VIEW))
#define MECH_GL_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_GL_VIEW, MechGLViewClass))

typedef struct _MechGLView MechGLView;
typedef struct _MechGLViewClass MechGLViewClass;

struct _MechGLView
{
  MechArea parent_instance;
};

struct _MechGLViewClass
{
  MechAreaClass parent_class;

  void       (* render_scene) (MechGLView     *view,
                               cairo_device_t *device);

  MechArea * (* pick_child)   (MechGLView *view,
                               gdouble     x,
                               gdouble     y,
                               gdouble    *child_x,
                               gdouble    *child_y);
  void       (* child_coords) (MechGLView *view,
                               MechArea   *child,
                               gdouble     x,
                               gdouble     y,
                               gdouble    *child_x,
                               gdouble    *child_y);
};

GType      mech_gl_view_get_type           (void) G_GNUC_CONST;

MechArea * mech_gl_view_new                (void);

gboolean   mech_gl_view_bind_child_texture (MechGLView *view,
                                            MechArea   *child);

G_END_DECLS

#endif /* __MECH_GL_VIEW_H__ */
