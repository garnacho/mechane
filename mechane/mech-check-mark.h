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

#ifndef __MECH_CHECK_MARK_H__
#define __MECH_CHECK_MARK_H__

#include <mechane/mech-area.h>
#include <mechane/mech-linear-box.h>

G_BEGIN_DECLS

#define MECH_TYPE_CHECK_MARK         (mech_check_mark_get_type ())
#define MECH_CHECK_MARK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_CHECK_MARK, MechCheckMark))
#define MECH_CHECK_MARK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_CHECK_MARK, MechCheckMarkClass))
#define MECH_IS_CHECK_MARK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_CHECK_MARK))
#define MECH_IS_CHECK_MARK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_CHECK_MARK))
#define MECH_CHECK_MARK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_CHECK_MARK, MechCheckMarkClass))

typedef struct _MechCheckMark MechCheckMark;
typedef struct _MechCheckMarkClass MechCheckMarkClass;

struct _MechCheckMark
{
  MechLinearBox parent_instance;
};

struct _MechCheckMarkClass
{
  MechLinearBoxClass parent_class;
};

GType            mech_check_mark_get_type (void) G_GNUC_CONST;
MechArea *       mech_check_mark_new      (void);

G_END_DECLS

#endif /* __MECH_CHECK_MARK_H__ */
