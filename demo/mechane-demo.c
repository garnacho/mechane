#include <mechane/mechane.h>
#include "mechane-demo-resources.h"

#include "matrices.c"
#include "scroll-box.c"
#include "scroll-area.c"
#include "gl.c"
#include "gl-box.c"

typedef struct _MechaneDemo MechaneDemo;
typedef struct _DemoData DemoData;

struct _DemoData {
  const gchar *name;
  const gchar *resource;
  const gchar *desc;
  MechArea * (* func) (void);
};

DemoData demos[] = {
  {
    "Matrices",
    "matrices.c",
    "Every non-root MechArea may have a 2D transformation matrix relative "
    "to its allocated position, which will also affect all child areas. "
    "The mech_area_transform_*() collection of functions can be used to "
    "translate points to the coordinate space of another MechArea.",
    demo_matrices
  },
  {
    "Scroll box",
    "scroll-box.c",
    "MechScrollBox is a MechArea that implements the fundamentals of "
    "scrolling. Each scroll axis is controlled by a MechAdjustable, "
    "so the viewport will update accordingly at each change. Internally, "
    "MechScrollBox uses mech_area_set_matrix() and mech_area_set_clip()",
    demo_scroll_box
  },
  { "Scroll area",
    "scroll-area.c",
    "MechScrollArea is a MechScrollBox implementation that provides "
    "scrollbars and allows the viewport to be manipulated by user "
    "input like scroll events and gestures.",
    demo_scroll_area
  },
  { "EGL",
    "gl.c",
    "When hardware acceleration is available, Mechane can make use of it "
    "quite transparently. This demo defines an area that does GL rendering "
    "on the draw() handler, 2 instances are then created within overlapped "
    "buttons.",
    demo_gl
  },
  { "GL box",
    "gl-box.c",
    "MechGLBox implements basic rendering capabilities of child areas onto "
    "GL textures. The position at which those textures are rendered is decided "
    "in the 'position-child' vmethod. MechGLBox also implements child picking "
    "and translation of coordinates for pointer events.",
    demo_gl_box
  }
};

enum {
  PANE_DEMO,
  PANE_INFO,
  PANE_CODE
};

struct _MechaneDemo {
  MechWindow *window;
  MechArea *code_button;
  MechArea *info_button;
  MechArea *content;

  MechTextView *view;
  MechArea *view_scroll;

  MechArea *current_demo;
  MechToggle *current_toggle;
  DemoData *current_demo_data;

  gint current_pane;
};

static MechaneDemo *global_data = NULL;

static gboolean
window_close_request (MechWindow *window,
                      gpointer    user_data)
{
  g_main_loop_quit (user_data);
  return FALSE;
}

static void
demo_set_description (MechTextView *view,
                      DemoData     *demo)
{
  MechTextIter title_start, title_end, iter;
  PangoFontDescription *font_desc;
  MechTextAttributes *attributes;
  MechTextBuffer *text_buffer;

  text_buffer = mech_text_buffer_new ();

  mech_text_buffer_get_bounds (text_buffer, &iter, NULL);
  mech_text_buffer_insert (text_buffer, &iter, demo->name, -1);
  mech_text_buffer_insert (text_buffer, &iter, "\n", -1);

  mech_text_buffer_insert (text_buffer, &iter, demo->desc, -1);
  mech_text_set_buffer (MECH_TEXT (view), text_buffer);

  /* Set title style */
  font_desc = pango_font_description_new ();
  pango_font_description_set_size (font_desc, 20 * PANGO_SCALE);

  attributes = mech_text_attributes_new ();
  g_object_set (attributes,
                "font-description", font_desc,
                NULL);
  pango_font_description_free (font_desc);

  mech_text_buffer_get_bounds (text_buffer, &title_start, NULL);
  mech_text_buffer_paragraph_extents (text_buffer, &title_start, NULL, &title_end);
  mech_text_view_combine_attributes (view, &title_start, &title_end, attributes);

  g_object_unref (attributes);
  g_object_unref (text_buffer);
}

static MechTextBuffer *
read_demo_resource (DemoData *demo)
{
#define BUFFER_SIZE 1024

  MechTextBuffer *text_buffer;
  gchar buffer[BUFFER_SIZE];
  GFileInputStream *stream;
  GFile *parent, *file;
  GError *error = NULL;
  gsize bytes_read;

  parent = g_file_new_for_uri ("resource://org/mechane/mechane-demo/demos/");
  file = g_file_get_child (parent, demo->resource);
  g_object_unref (parent);

  stream = g_file_read (file, NULL, &error);

  if (error)
    {
      g_warning ("Could not load demo resource: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  text_buffer = mech_text_buffer_new ();

  while ((bytes_read = g_input_stream_read (G_INPUT_STREAM (stream),
                                            buffer, BUFFER_SIZE,
                                            NULL, &error)) > 0)
    {
      MechTextIter iter;

      mech_text_buffer_get_bounds (text_buffer, NULL, &iter);
      mech_text_buffer_insert (text_buffer, &iter, buffer, bytes_read);
    }

  if (error)
    {
      g_warning ("Could not load demo resource: %s\n", error->message);
      g_error_free (error);
    }

  return text_buffer;

#undef BUFFER_SIZE
}

static void
update_content_pane (void)
{
  if (!global_data->current_demo_data)
    return;

  if (global_data->current_pane == PANE_DEMO)
    {
      if (global_data->view_scroll)
        {
          mech_area_remove (global_data->content,
                            MECH_AREA (global_data->view_scroll));
          global_data->view_scroll = NULL;
          global_data->view = NULL;
        }

      mech_area_add (global_data->content,
                     global_data->current_demo);
    }
  else
    {
      if (global_data->current_demo &&
          mech_area_get_parent (global_data->current_demo))
        mech_area_remove (global_data->content,
                          global_data->current_demo);

      if (global_data->view_scroll &&
          mech_area_get_parent (global_data->view_scroll))
        mech_area_remove (global_data->content,
                          global_data->view_scroll);

      global_data->view = (MechTextView *) mech_text_view_new ();
      global_data->view_scroll = mech_scroll_area_new (MECH_AXIS_FLAG_Y);
      mech_area_add (global_data->view_scroll,
                     MECH_AREA (global_data->view));
      mech_area_add (global_data->content,
                     MECH_AREA (global_data->view_scroll));

      if (global_data->current_pane == PANE_INFO)
        demo_set_description (global_data->view,
                              global_data->current_demo_data);
      else if (global_data->current_pane == PANE_CODE)
        {
          MechTextBuffer *buffer;

          mech_area_set_name ((MechArea *) global_data->view, "code");
          buffer = read_demo_resource (global_data->current_demo_data);

          if (buffer)
            {
              mech_text_set_buffer (MECH_TEXT (global_data->view), buffer);
              g_object_unref (buffer);
            }
          else
            mech_text_set_string (MECH_TEXT (global_data->view),
                                  "Error loading demo code");
        }
    }
}

static void
demo_selected (MechActivatable *activatable,
               DemoData        *demo)
{
  if (global_data->current_toggle &&
      global_data->current_toggle != (MechToggle *) activatable)
    mech_toggle_set_active (MECH_TOGGLE (global_data->current_toggle), FALSE);

  if (global_data->current_demo)
    {
      if (global_data->current_demo &&
          mech_area_get_parent (global_data->current_demo))
        mech_area_remove (global_data->content,
                          global_data->current_demo);

      g_object_unref (global_data->current_demo);
    }

  if (global_data->view_scroll &&
      mech_area_get_parent (global_data->view_scroll))
    mech_area_remove (global_data->content,
                      global_data->view_scroll);

  global_data->current_demo = NULL;
  global_data->current_demo_data = NULL;
  global_data->current_toggle = NULL;
  global_data->view_scroll = NULL;
  global_data->view = NULL;

  if (mech_toggle_get_active (MECH_TOGGLE (activatable)))
    {
      global_data->current_demo_data = demo;
      global_data->current_demo = demo->func ();
      global_data->current_toggle = (MechToggle *) activatable;

      /* Keep an extra ref so it survives across reparents */
      g_object_ref (global_data->current_demo);
    }

  update_content_pane ();
}

static MechArea *
create_demo_list (void)
{
  MechArea *pane, *scroll, *box, *button, *text;
  gint i;

  pane = mech_area_new ("list-pane", MECH_NONE);
  scroll = mech_scroll_area_new (MECH_AXIS_FLAG_Y);
  mech_area_add (pane, scroll);

  box = mech_linear_box_new (MECH_ORIENTATION_VERTICAL);
  mech_area_add (scroll, box);

  for (i = 0; i < G_N_ELEMENTS (demos); i++)
    {
      button = mech_toggle_button_new ();
      text = mech_text_view_new ();
      mech_text_set_string (MECH_TEXT (text), demos[i].name);
      mech_area_add (button, text);
      mech_area_add (box, button);

      g_signal_connect (button, "toggled",
                        G_CALLBACK (demo_selected), &demos[i]);
    }

  return pane;
}

static void
info_button_toggled (MechActivatable *activatable)
{
  if (mech_toggle_get_active (MECH_TOGGLE (activatable)))
    {
      mech_toggle_set_active (MECH_TOGGLE (global_data->code_button), FALSE);
      global_data->current_pane = PANE_INFO;
    }
  else
    global_data->current_pane = PANE_DEMO;

  update_content_pane ();
}

static void
code_button_toggled (MechActivatable *activatable)
{
  if (mech_toggle_get_active (MECH_TOGGLE (activatable)))
    {
      mech_toggle_set_active (MECH_TOGGLE (global_data->info_button), FALSE);
      global_data->current_pane = PANE_CODE;
    }
  else
    global_data->current_pane = PANE_DEMO;

  update_content_pane ();
}

static MechArea *
create_buttons (void)
{
  MechArea *box;

  box = mech_linear_box_new (MECH_ORIENTATION_HORIZONTAL);

  global_data->info_button = mech_toggle_button_new ();
  mech_area_set_name (global_data->info_button, "info-button");
  mech_area_add (box, global_data->info_button);
  g_signal_connect_after (global_data->info_button, "toggled",
                          G_CALLBACK (info_button_toggled), NULL);

  global_data->code_button = mech_toggle_button_new ();
  mech_area_set_name (global_data->code_button, "code-button");
  mech_area_add (box, global_data->code_button);
  g_signal_connect_after (global_data->code_button, "toggled",
                          G_CALLBACK (code_button_toggled), NULL);

  return box;
}

static void
load_demo_style (MechWindow *window)
{
  GError *error = NULL;
  MechTheme *theme;
  GFile *file;

  theme = mech_theme_new ();
  file = g_file_new_for_uri ("resource://org/mechane/mechane-demo/style");
  mech_theme_load_style_from_file (theme,
                                   mech_window_get_style (window),
                                   file, &error);

  if (error)
    {
      g_warning ("Could not load demo style: %s\n", error->message);
      g_error_free (error);
    }

  g_object_unref (theme);
  g_object_unref (file);
}

MechWindow *
create_main_window (void)
{
  MechArea *list, *box, *buttons;

  global_data = g_new0 (MechaneDemo, 1);

  global_data->window = mech_window_new ();
  load_demo_style (global_data->window);
  mech_window_set_title (global_data->window, "Mechane demo");

  box = mech_fixed_box_new ();
  mech_area_add (mech_container_get_root (MECH_CONTAINER (global_data->window)),
                 box);
  mech_area_set_preferred_size (box, MECH_AXIS_X, MECH_UNIT_PX, 800);
  mech_area_set_preferred_size (box, MECH_AXIS_Y, MECH_UNIT_PX, 500);

  list = create_demo_list ();
  mech_area_add (box, list);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), list,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT, MECH_UNIT_PX, 0);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), list,
                         MECH_SIDE_TOP, MECH_SIDE_TOP, MECH_UNIT_PX, 6);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), list,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 6);
  mech_area_set_preferred_size (list, MECH_AXIS_X, MECH_UNIT_PX, 200.);

  global_data->content = mech_area_new ("demo-pane", MECH_NONE);
  mech_area_set_clip (global_data->content, TRUE);
  mech_area_add (box, global_data->content);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), global_data->content,
                         MECH_SIDE_LEFT, MECH_SIDE_LEFT, MECH_UNIT_PX, 210);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), global_data->content,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 6);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), global_data->content,
                         MECH_SIDE_TOP, MECH_SIDE_TOP, MECH_UNIT_PX, 6);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), global_data->content,
                         MECH_SIDE_BOTTOM, MECH_SIDE_BOTTOM, MECH_UNIT_PX, 6);

  buttons = create_buttons ();
  mech_area_add (box, buttons);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), buttons,
                         MECH_SIDE_RIGHT, MECH_SIDE_RIGHT, MECH_UNIT_PX, 14);
  mech_fixed_box_attach (MECH_FIXED_BOX (box), buttons,
                         MECH_SIDE_TOP, MECH_SIDE_TOP, MECH_UNIT_PX, 8);

  return global_data->window;
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *main_loop;
  MechWindow *window;

  main_loop = g_main_loop_new (NULL, FALSE);
  window = create_main_window ();
  mech_window_set_visible (window, TRUE);
  g_signal_connect (window, "close-request",
                    G_CALLBACK (window_close_request), main_loop);

  g_main_loop_run (main_loop);

  g_object_unref (window);

  return 0;
}
