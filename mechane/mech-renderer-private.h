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

#ifndef __MECH_RENDERER_PRIVATE_H__
#define __MECH_RENDERER_PRIVATE_H__

#include <mechane/mech-enums.h>
#include "mech-pattern-private.h"
#include "mech-renderer.h"

G_BEGIN_DECLS

typedef enum {
  MECH_RENDERER_COPY_BACKGROUND,
  MECH_RENDERER_COPY_FOREGROUND,
  MECH_RENDERER_COPY_BORDER
} MechRendererCopyFlags;

MechRenderer * _mech_renderer_new                    (void);

MechRenderer * _mech_renderer_copy                   (MechRenderer          *renderer,
                                                      MechRendererCopyFlags  flags);

void           _mech_renderer_set_corner_radius      (MechRenderer          *renderer,
                                                      guint                  corners,
                                                      gdouble                radius);

gint           _mech_renderer_add_background         (MechRenderer          *renderer,
                                                      MechPattern           *pattern);
gint           _mech_renderer_add_border             (MechRenderer          *renderer,
                                                      MechPattern           *pattern,
                                                      MechBorder            *border);
gint           _mech_renderer_add_foreground         (MechRenderer          *renderer,
                                                      MechPattern           *pattern);
void           _mech_renderer_set_font_family        (MechRenderer          *renderer,
                                                      const gchar           *family);
void           _mech_renderer_set_font_size          (MechRenderer          *renderer,
                                                      gdouble                size,
                                                      MechUnit               unit);
void           _mech_renderer_set_padding            (MechRenderer          *renderer,
                                                      MechBorder            *border);
void           _mech_renderer_set_margin             (MechRenderer          *renderer,
                                                      MechBorder            *border);

G_END_DECLS

#endif /* __MECH_RENDERER_PRIVATE_H__ */
