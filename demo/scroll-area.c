#include <mechane/mechane.h>

MechArea *
demo_scroll_area (void)
{
  MechArea *scroll, *bbox;
  guint i;

  scroll = mech_scroll_area_new (MECH_AXIS_FLAG_X | MECH_AXIS_FLAG_Y);

  bbox = mech_floating_box_new ();
  mech_area_set_preferred_size (bbox, MECH_AXIS_X, MECH_UNIT_PX, 1000);
  mech_area_add (scroll, bbox);

  for (i = 0; i < 100; i++)
    {
      MechArea *button, *text;
      gchar *str;

      str = g_strdup_printf ("Button %d", i + 1);

      button = mech_button_new ();
      mech_area_add (bbox, button);

      text = mech_text_view_new ();
      mech_text_set_string (MECH_TEXT (text), str);
      mech_area_add (button, text);

      mech_area_set_preferred_size (button, MECH_AXIS_X,
                                    MECH_UNIT_PX, 250);
      mech_area_set_preferred_size (button, MECH_AXIS_Y,
                                    MECH_UNIT_PX, 250);

      g_free (str);
    }

  return scroll;
}
