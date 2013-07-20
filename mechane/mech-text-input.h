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

#ifndef __MECH_TEXT_INPUT_H__
#define __MECH_TEXT_INPUT_H__

#include <mechane/mech-text-view.h>

G_BEGIN_DECLS

#define MECH_TYPE_TEXT_INPUT         (mech_text_input_get_type ())
#define MECH_TEXT_INPUT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MECH_TYPE_TEXT_INPUT, MechTextInput))
#define MECH_TEXT_INPUT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MECH_TYPE_TEXT_INPUT, MechTextInputClass))
#define MECH_IS_TEXT_INPUT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MECH_TYPE_TEXT_INPUT))
#define MECH_IS_TEXT_INPUT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MECH_TYPE_TEXT_INPUT))
#define MECH_TEXT_INPUT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MECH_TYPE_TEXT_INPUT, MechTextInputClass))

typedef struct _MechTextInput MechTextInput;
typedef struct _MechTextInputClass MechTextInputClass;

struct _MechTextInput
{
  MechTextView parent_instance;
};

struct _MechTextInputClass
{
  MechTextViewClass parent_class;
};

GType            mech_text_input_get_type (void) G_GNUC_CONST;
MechArea *       mech_text_input_new      (void);

G_END_DECLS

#endif /* __MECH_TEXT_INPUT_H__ */
