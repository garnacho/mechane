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

#include <mechane/mech-area.h>

#ifndef __MECH_EVENTS_H__
#define __MECH_EVENTS_H__

#include <mechane/mech-seat.h>

G_BEGIN_DECLS

typedef struct _MechEventAny MechEventAny;
typedef struct _MechEventInput MechEventInput;
typedef struct _MechEventKey MechEventKey;
typedef struct _MechEventPointer MechEventPointer;
typedef struct _MechEventScroll MechEventScroll;
typedef struct _MechEventCrossing MechEventCrossing;
typedef struct _MechEventTouch MechEventTouch;
typedef union _MechEvent MechEvent;

#define MECH_TYPE_EVENT (mech_event_get_type ())

typedef enum {
  MECH_CROSSING_NORMAL,
  MECH_CROSSING_GRAB,
  MECH_CROSSING_UNGRAB,
} MechCrossingMode;

typedef enum {
  MECH_EVENT_FLAG_NONE              = 0,
  MECH_EVENT_FLAG_SYNTHESIZED       = 1 << 0,
  MECH_EVENT_FLAG_COMPRESSED        = 1 << 1,
  MECH_EVENT_FLAG_CROSSING_OBSCURED = 1 << 2,
  MECH_EVENT_FLAG_KEY_IS_MODIFIER   = 1 << 3,
  MECH_EVENT_FLAG_KEY_IS_REPEAT     = 1 << 4,
  MECH_EVENT_FLAG_TOUCH_EMULATED    = 1 << 5,
  MECH_EVENT_FLAG_CAPTURE_PHASE     = 1 << 6
} MechEventFlags;

struct _MechEventAny
{
  MechEventType type;
  guint32 serial;
  MechArea *area;
  MechArea *target;
  MechSeat *seat;
  guint32 evtime;
  guint16 flags;
};

struct _MechEventCrossing
{
  MechEventAny any;
  MechArea *other_area;
  guint mode;
};

struct _MechEventInput
{
  MechEventAny any;
  guint32 evtime;
  guint16 modifiers;
};

struct _MechEventKey
{
  MechEventInput input;
  guint32 keycode;
  guint32 keyval;
  gunichar unicode_char;
};

struct _MechEventPointer
{
  MechEventInput input;
  gdouble *axes;
  gdouble x;
  gdouble y;
};

struct _MechEventScroll
{
  MechEventPointer pointer;
  gdouble dx;
  gdouble dy;
};

struct _MechEventTouch
{
  MechEventPointer pointer;
  gint32 id;
};

union _MechEvent
{
  MechEventType type;
  MechEventAny any;
  MechEventInput input;
  MechEventCrossing focus, crossing;
  MechEventKey key;
  MechEventPointer pointer;
  MechEventScroll scroll;
  MechEventTouch touch;
};

GType       mech_event_get_type            (void) G_GNUC_CONST;

MechEvent * mech_event_new                 (MechEventType   type);
MechEvent * mech_event_translate           (MechEvent      *event,
                                            MechArea       *area);
MechEvent * mech_event_copy                (MechEvent      *event);

void        mech_event_free                (MechEvent      *event);

MechArea  * mech_event_get_area            (MechEvent      *event);
void        mech_event_set_area            (MechEvent      *event,
                                            MechArea       *area);

MechArea *  mech_event_get_target          (MechEvent      *event);
void        mech_event_set_target          (MechEvent      *event,
                                            MechArea       *target);

MechSeat  * mech_event_get_seat            (MechEvent      *event);
void        mech_event_set_seat            (MechEvent      *event,
                                            MechSeat       *seat);
gboolean    mech_event_has_flags           (MechEvent      *input,
                                            guint           flags);

gboolean    mech_event_input_get_time      (MechEvent      *event,
                                            guint32        *evtime);

gboolean    mech_event_pointer_get_coords  (MechEvent      *event,
                                            gdouble        *x,
                                            gdouble        *y);
gboolean    mech_event_scroll_get_deltas   (MechEvent      *event,
                                            gdouble        *dx,
                                            gdouble        *dy);
gboolean    mech_event_crossing_get_areas  (MechEvent      *event,
                                            MechArea      **from,
                                            MechArea      **to);
gboolean    mech_event_crossing_get_mode   (MechEvent      *event,
                                            guint          *mode);
gboolean    mech_event_touch_get_id        (MechEvent      *event,
                                            gint32         *id);

G_END_DECLS

#endif /* __MECH_EVENTS_H__ */
