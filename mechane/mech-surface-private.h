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

#ifndef __MECH_SURFACE_PRIVATE_H__
#define __MECH_SURFACE_PRIVATE_H__

#include <mechane/mech-area.h>
#include <cairo/cairo.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_SURFACE         (mech_surface_get_type ())
#define MECH_SURFACE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_SURFACE, MechSurface))
#define MECH_SURFACE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_SURFACE, MechSurfaceClass))
#define MECH_IS_SURFACE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_SURFACE))
#define MECH_IS_SURFACE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_SURFACE))
#define MECH_SURFACE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_SURFACE, MechSurfaceClass))

typedef struct _MechSurface MechSurface;
typedef struct _MechSurfaceClass MechSurfaceClass;

struct _MechSurface
{
  GObject parent_instance;
};

struct _MechSurfaceClass
{
  GObjectClass parent_class;

  void              (* set_size)    (MechSurface *surface,
                                     gint         width,
                                     gint         height);
  cairo_surface_t * (* get_surface) (MechSurface *surface);
};

GType            mech_surface_get_type          (void) G_GNUC_CONST;

MechSurface    * _mech_surface_new              (MechArea          *area);

void             _mech_surface_set_size         (MechSurface       *surface,
                                                 gint               width,
                                                 gint               height);
void             _mech_surface_get_size         (MechSurface       *surface,
                                                 gint              *width,
                                                 gint              *height);

cairo_t        * _mech_surface_cairo_create     (MechSurface       *surface);

void             _mech_surface_set_source       (cairo_t           *cr,
                                                 MechSurface       *surface);

void             _mech_surface_damage           (MechSurface       *surface,
                                                 cairo_rectangle_t *rect);
cairo_region_t * _mech_surface_apply_clip       (MechSurface       *surface,
                                                 cairo_t           *cr);

G_END_DECLS

#endif /* __MECH_SURFACE_PRIVATE_H__ */
