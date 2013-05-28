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

#ifndef __MECH_IMAGE_H__
#define __MECH_IMAGE_H__

#include <gio/gio.h>
#include <mechane/mech-area.h>

G_BEGIN_DECLS

#define MECH_TYPE_IMAGE         (mech_image_get_type ())
#define MECH_IMAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_IMAGE, MechImage))
#define MECH_IMAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_IMAGE, MechImageClass))
#define MECH_IS_IMAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_IMAGE))
#define MECH_IS_IMAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_IMAGE))
#define MECH_IMAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_IMAGE, MechImageClass))

typedef struct _MechImage MechImage;
typedef struct _MechImageClass MechImageClass;

struct _MechImage
{
  MechArea parent_instance;
};

struct _MechImageClass
{
  MechAreaClass parent_class;
};

GType      mech_image_get_type      (void) G_GNUC_CONST;
MechArea * mech_image_new           (void);
MechArea * mech_image_new_from_file (GFile     *file);

GFile *    mech_image_get_file      (MechImage *image);
void       mech_image_set_file      (MechImage *image,
                                     GFile     *file);

G_END_DECLS

#endif /* __MECH_IMAGE_H__ */
