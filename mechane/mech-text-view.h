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

#ifndef __MECH_TEXT_VIEW_H__
#define __MECH_TEXT_VIEW_H__

#include <mechane/mech-view.h>
#include <mechane/mech-text-buffer.h>
#include <mechane/mech-text-attributes.h>

G_BEGIN_DECLS

#define MECH_TYPE_TEXT_VIEW         (mech_text_view_get_type ())
#define MECH_TEXT_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_TEXT_VIEW, MechTextView))
#define MECH_TEXT_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_TEXT_VIEW, MechTextViewClass))
#define MECH_IS_TEXT_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_TEXT_VIEW))
#define MECH_IS_TEXT_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_TEXT_VIEW))
#define MECH_TEXT_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_TEXT_VIEW, MechTextViewClass))

typedef struct _MechTextView MechTextView;
typedef struct _MechTextViewClass MechTextViewClass;

struct _MechTextView
{
  MechView parent_instance;
};

struct _MechTextViewClass
{
  MechViewClass parent_class;
};

GType                mech_text_view_get_type             (void) G_GNUC_CONST;
MechArea *           mech_text_view_new                  (void);

/* Text style */
void                 mech_text_view_combine_attributes   (MechTextView            *view,
                                                          MechTextIter            *start,
                                                          MechTextIter            *end,
                                                          MechTextAttributes      *attributes);
void                 mech_text_view_unset_attributes     (MechTextView            *view,
                                                          MechTextIter            *start,
                                                          MechTextIter            *end,
                                                          MechTextAttributeFields  fields);
MechTextAttributes * mech_text_view_get_attributes       (MechTextView            *view,
                                                          MechTextIter            *iter);

G_END_DECLS

#endif /* __MECH_TEXT_VIEW_H__ */
