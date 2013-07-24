#include <mechane/mechane.h>

static gboolean
scroll_box_handle_event (MechArea  *area,
                         MechEvent *event)
{
  if (event->type == MECH_MOTION)
    {
      gdouble x, y, min, max, view_size;
      MechAdjustable *adjustable;
      cairo_rectangle_t alloc;

      mech_area_get_allocated_size (area, &alloc);
      mech_event_pointer_get_coords (event, &x, &y);

      /* Set x/y, relative to min..max-view_size */
      adjustable = mech_scrollable_get_adjustable (MECH_SCROLLABLE (area),
                                                   MECH_AXIS_X);
      mech_adjustable_get_bounds (adjustable, &min, &max);
      view_size = mech_adjustable_get_selection_size (adjustable);
      mech_adjustable_set_value (adjustable, min + (x * (max - min - view_size) /
                                                    alloc.width));

      adjustable = mech_scrollable_get_adjustable (MECH_SCROLLABLE (area),
                                                   MECH_AXIS_Y);
      mech_adjustable_get_bounds (adjustable, NULL, &max);
      mech_adjustable_set_value (adjustable, min + (y * (max - min - view_size) /
                                                    alloc.height));

      return TRUE;
    }

  return FALSE;
}

MechArea *
demo_scroll_box (void)
{
  MechAdjustable *adjustable;
  MechArea *scroll, *area;
  GFile *file;

  scroll = mech_scroll_box_new ();
  mech_area_add_events (scroll, MECH_MOTION_MASK);
  g_signal_connect (scroll, "handle-event",
                    G_CALLBACK (scroll_box_handle_event), NULL);

  adjustable = MECH_ADJUSTABLE (mech_slider_new (MECH_ORIENTATION_HORIZONTAL));
  mech_scrollable_set_adjustable (MECH_SCROLLABLE (scroll),
                                  MECH_AXIS_X, adjustable);

  adjustable = MECH_ADJUSTABLE (mech_slider_new (MECH_ORIENTATION_VERTICAL));
  mech_scrollable_set_adjustable (MECH_SCROLLABLE (scroll),
                                  MECH_AXIS_Y, adjustable);

  /* Add background */
  file = g_file_new_for_uri ("resource://org/mechane/mechane-demo/map.jpg");
  area = mech_image_new_from_file (file);
  mech_area_add (scroll, area);
  g_object_unref (file);

  /* Add two layers of semi-transparent clouds, set a bigger size
   * explicitly to create a pseudo-3D effect. The clouds are done
   * via theming so the patterns can be extended
   */
  area = mech_area_new ("cloud", MECH_NONE);
  mech_area_set_preferred_size (area, MECH_AXIS_X, MECH_UNIT_PX, 2000);
  mech_area_set_preferred_size (area, MECH_AXIS_Y, MECH_UNIT_PX, 2000);
  mech_area_add (scroll, area);

  area = mech_area_new ("cloud2", MECH_NONE);
  mech_area_set_preferred_size (area, MECH_AXIS_X, MECH_UNIT_PX, 4000);
  mech_area_set_preferred_size (area, MECH_AXIS_Y, MECH_UNIT_PX, 4000);
  mech_area_add (scroll, area);

  return scroll;
}
