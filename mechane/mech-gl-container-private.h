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

#ifndef __MECH_GL_CONTAINER_H__
#define __MECH_GL_CONTAINER_H__

#include <mechane/mech-container.h>
#include <mechane/mech-events.h>

G_BEGIN_DECLS

#define MECH_TYPE_GL_CONTAINER         (mech_gl_container_get_type ())
#define MECH_GL_CONTAINER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_GL_CONTAINER, MechGLContainer))
#define MECH_GL_CONTAINER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_GL_CONTAINER, MechGLContainerClass))
#define MECH_IS_GL_CONTAINER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_GL_CONTAINER))
#define MECH_IS_GL_CONTAINER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_GL_CONTAINER))
#define MECH_GL_CONTAINER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_GL_CONTAINER, MechGLContainerClass))

typedef struct _MechGLContainer MechGLContainer;
typedef struct _MechGLContainerClass MechGLContainerClass;

struct _MechGLContainer
{
  MechContainer parent_instance;
};

struct _MechGLContainerClass
{
  MechContainerClass parent_class;
};

GType             mech_gl_container_get_type        (void) G_GNUC_CONST;

MechGLContainer * _mech_gl_container_new            (MechArea        *parent);
guint             _mech_gl_container_get_texture_id (MechGLContainer *container);

G_END_DECLS

#endif /* __MECH_GL_CONTAINER_H__ */
