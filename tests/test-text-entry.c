#include <mechane/mechane.h>

static gboolean
window_close_request (MechWindow *window,
                      gpointer    user_data)
{
  g_main_loop_quit (user_data);
  return FALSE;
}

static MechTextBuffer *
create_buffer (guint n_paragraphs)
{
  GTimer *timer = g_timer_new ();
  MechTextBuffer *buffer;
  gchar *formatted_size;
  gsize size;
  guint i;

  buffer = mech_text_buffer_new ();

  for (i = 0; i < n_paragraphs; i++)
    {
      MechTextIter iter;
      gchar *str;

      mech_text_buffer_get_bounds (buffer, NULL, &iter);
      str = g_strdup_printf ("Paragraph %d: Lorem ipsum dolor sit amet, consectetur "
                             "elit. Sed fermentum accumsan nisi, nec commodo neque "
                             "dictum vitae. Suspendisse vitae ligula felis, id "
                             "dictum risus. Suspendisse semper eros ac massa "
                             "imperdiet sagittis. Vestibulum sem dolor, lobortis "
                             "sit amet lobortis porta, cursus a sem. Nam mattis "
                             "rutrum dignissim. Phasellus hendrerit mauris eget "
                             "massa placerat luctus. Nam rutrum dapibus justo ac "
                             "ornare. Phasellus adipiscing tempor massa, vel "
                             "ultricies nulla fermentum non. Proin in lacus "
                             "bibendum lectus aliquam gravida eget id erat. "
                             "Etiam tincidunt sodales urna, eget fermentum orci "
                             "venenatis eu. Donec id sollicitudin augue. Ut turpis "
                             "odio, convallis sit amet euismod eu, elementum non "
                             "neque.\n\n", i);
      mech_text_buffer_insert (buffer, &iter, str, -1);
      g_free (str);
    }

  size = mech_text_buffer_get_byte_offset (buffer, NULL, NULL);
  formatted_size = g_format_size (size);

  g_print ("Buffer with %d paragraphs created in %f, %ld bytes (%s)\n",
           n_paragraphs, g_timer_elapsed (timer, NULL),
           mech_text_buffer_get_byte_offset (buffer, NULL, NULL),
           formatted_size);

  g_timer_destroy (timer);
  g_free (formatted_size);

  return buffer;
}

int
main (int argc, char *argv[])
{
  MechArea *scroll, *text;
  MechWindow *window;
  MechTextBuffer *buffer;
  GMainLoop *main_loop;
  gint n_paragraphs = 0;

  if (argc > 1)
    n_paragraphs = atoi (argv[1]);

  if (n_paragraphs <= 0)
    {
      gchar *basename;

      basename = g_filename_display_basename (argv[0]);
      g_print ("Usage: %s <n-paragraphs>\n\n", basename);
      n_paragraphs = 10000;
      g_free (basename);
    }

  main_loop = g_main_loop_new (NULL, FALSE);

  window = mech_window_new ();
  mech_window_set_title (window, "Text");
  g_signal_connect (window, "close-request",
                    G_CALLBACK (window_close_request), main_loop);

  scroll = mech_scroll_area_new (MECH_AXIS_FLAG_Y);
  mech_area_set_preferred_size (scroll, MECH_AXIS_X, MECH_UNIT_PX, 500);
  mech_area_set_preferred_size (scroll, MECH_AXIS_Y, MECH_UNIT_PX, 500);
  mech_area_add (mech_container_get_root (MECH_CONTAINER (window)), scroll);

  text = mech_text_input_new ();
  mech_area_add (scroll, text);

  buffer = create_buffer (n_paragraphs);
  mech_text_set_buffer (MECH_TEXT (text), buffer);

  mech_window_set_visible (window, TRUE);
  g_main_loop_run (main_loop);

  return 0;
}
