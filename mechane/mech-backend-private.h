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

#ifndef __MECH_BACKEND_H__
#define __MECH_BACKEND_H__

#include <mechane/mech-surface-private.h>
#include <mechane/mech-window.h>
#include <mechane/mechane.h>

G_BEGIN_DECLS

#define MECH_TYPE_BACKEND         (mech_backend_get_type ())
#define MECH_BACKEND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_BACKEND, MechBackend))
#define MECH_BACKEND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_BACKEND, MechBackendClass))
#define MECH_IS_BACKEND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_BACKEND))
#define MECH_IS_BACKEND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_BACKEND))
#define MECH_BACKEND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_BACKEND, MechBackendClass))

typedef struct _MechBackend MechBackend;
typedef struct _MechBackendClass MechBackendClass;

struct _MechBackend
{
  GObject parent_instance;
};

struct _MechBackendClass
{
  GObjectClass parent_class;

  MechWindow  * (* create_window)  (MechBackend    *backend);
  MechSurface * (* create_surface) (MechBackend    *backend);
};

GType         mech_backend_get_type       (void) G_GNUC_CONST;

MechBackend * mech_backend_get            (void);
MechWindow  * mech_backend_create_window  (MechBackend    *backend);
MechSurface * mech_backend_create_surface (MechBackend    *backend);

G_END_DECLS

#endif /* __MECH_BACKEND_H__ */
