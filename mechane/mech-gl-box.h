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

#ifndef __MECH_GL_BOX_H__
#define __MECH_GL_BOX_H__

#include <mechane/mech-gl-view.h>

G_BEGIN_DECLS

#define MECH_TYPE_GL_BOX         (mech_gl_box_get_type ())
#define MECH_GL_BOX(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_GL_BOX, MechGLBox))
#define MECH_GL_BOX_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_GL_BOX, MechGLBoxClass))
#define MECH_IS_GL_BOX(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_GL_BOX))
#define MECH_IS_GL_BOX_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_GL_BOX))
#define MECH_GL_BOX_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_GL_BOX, MechGLBoxlass))

typedef struct _MechGLBox MechGLBox;
typedef struct _MechGLBoxClass MechGLBoxClass;

struct _MechGLBox
{
  MechGLView parent_instance;
};

struct _MechGLBoxClass
{
  MechGLViewClass parent_class;

  void     (* position_child) (MechGLBox *box,
                               MechArea  *child);
};

GType      mech_gl_box_get_type           (void) G_GNUC_CONST;

MechArea * mech_gl_box_new                (void);

G_END_DECLS

#endif /* __MECH_GL_BOX_H__ */
