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

#ifndef __MECH_RENDERER_H__
#define __MECH_RENDERER_H__

#include <pango/pango.h>
#include <mechane/mech-types.h>

G_BEGIN_DECLS

#define MECH_TYPE_RENDERER         (mech_renderer_get_type ())
#define MECH_RENDERER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_RENDERER, MechRenderer))
#define MECH_RENDERER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_RENDERER, MechRendererClass))
#define MECH_IS_RENDERER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_RENDERER))
#define MECH_IS_RENDERER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_RENDERER))
#define MECH_RENDERER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_RENDERER, MechRendererClass))

typedef struct _MechRenderer MechRenderer;
typedef struct _MechRendererClass MechRendererClass;

struct _MechRenderer
{
  GObject parent_instance;
};

struct _MechRendererClass
{
  GObjectClass parent_class;
};

typedef enum {
  MECH_EXTENT_MARGIN,
  MECH_EXTENT_BORDER,
  MECH_EXTENT_PADDING,
  MECH_EXTENT_CONTENT
} MechExtentType;

GType          mech_renderer_get_type           (void) G_GNUC_CONST;

PangoLayout *  mech_renderer_create_layout      (MechRenderer    *renderer,
                                                 const gchar     *text);
PangoContext * mech_renderer_get_font_context   (MechRenderer    *renderer);

void           mech_renderer_set_border_path    (MechRenderer    *renderer,
                                                 cairo_t         *cr,
                                                 gdouble          x,
                                                 gdouble          y,
                                                 gdouble          width,
                                                 gdouble          height);
void           mech_renderer_render_background  (MechRenderer    *renderer,
                                                 cairo_t         *cr,
                                                 gdouble          x,
                                                 gdouble          y,
                                                 gdouble          width,
                                                 gdouble          height);
void           mech_renderer_render_border      (MechRenderer    *renderer,
                                                 cairo_t         *cr,
                                                 gdouble          x,
                                                 gdouble          y,
                                                 gdouble          width,
                                                 gdouble          height);
void           mech_renderer_render_layout      (MechRenderer    *renderer,
                                                 cairo_t         *cr,
                                                 PangoLayout     *layout);

void           mech_renderer_get_border_extents (MechRenderer    *renderer,
                                                 MechExtentType   extent_type,
                                                 MechBorder      *border);

G_END_DECLS

#endif /* __MECH_RENDERER_H__ */
