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

#ifndef __MECH_TEXT_RANGE_H__
#define __MECH_TEXT_RANGE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MECH_TYPE_TEXT_RANGE         (mech_text_range_get_type ())
#define MECH_TEXT_RANGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_TEXT_RANGE, MechTextRange))
#define MECH_TEXT_RANGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_TEXT_RANGE, MechTextRangeClass))
#define MECH_IS_TEXT_RANGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_TEXT_RANGE))
#define MECH_IS_TEXT_RANGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_TEXT_RANGE))
#define MECH_TEXT_RANGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_TEXT_RANGE, MechTextRangeClass))

typedef struct _MechTextRange MechTextRange;
typedef struct _MechTextRangeClass MechTextRangeClass;

struct _MechTextRange
{
  GObject parent_instance;
};

struct _MechTextRangeClass
{
  GObjectClass parent_class;

  void (* added)   (MechTextRange *range,
                    MechTextIter  *start,
                    MechTextIter  *end);
  void (* removed) (MechTextRange *range,
                    MechTextIter  *start,
                    MechTextIter  *end);
};

GType           mech_text_range_get_type (void) G_GNUC_CONST;

MechTextRange * mech_text_range_new        (void);

void            mech_text_range_set_bounds (MechTextRange      *range,
                                            const MechTextIter *start,
                                            const MechTextIter *end);

gboolean        mech_text_range_get_bounds (MechTextRange      *range,
                                            MechTextIter       *start,
                                            MechTextIter       *end);

G_END_DECLS

#endif /* __MECH_TEXT_RANGE_H__ */
