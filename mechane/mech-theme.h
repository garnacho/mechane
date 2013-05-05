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

#ifndef __MECH_THEME_H__
#define __MECH_THEME_H__

#include <gio/gio.h>
#include <mechane/mechane.h>
#include <mechane/mech-style.h>

G_BEGIN_DECLS

#define MECH_TYPE_THEME         (mech_theme_get_type ())
#define MECH_THEME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_THEME, MechTheme))
#define MECH_THEME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_THEME, MechThemeClass))
#define MECH_IS_THEME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_THEME))
#define MECH_IS_THEME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_THEME))
#define MECH_THEME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_THEME, MechThemeClass))

typedef struct _MechTheme MechTheme;
typedef struct _MechThemeClass MechThemeClass;

struct _MechTheme
{
  GObject parent_instance;
};

struct _MechThemeClass
{
  GObjectClass parent_class;
};

GType       mech_theme_get_type             (void) G_GNUC_CONST;

MechTheme * mech_theme_new                  (void);

gboolean    mech_theme_load_style_from_data (MechTheme    *theme,
                                             MechStyle    *style,
                                             const gchar  *data,
                                             gssize        len,
                                             GError      **error);
gboolean    mech_theme_load_style_from_file (MechTheme    *theme,
                                             MechStyle    *style,
                                             GFile        *file,
                                             GError      **error);

G_END_DECLS

#endif /* __MECH_THEME_H__ */
