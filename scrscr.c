// scrscr - screenshot annotation tool

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

typedef char bool;
#define true 1
#define false 0
#define nil NULL

char* fn;
void *window, *vbox, *vbox2, *swindow, *hbox, *hbox2, *image, *image_evbox;
int width, height;
GdkPixbuf *pixbuf;
char* pixbuf_copy;
cairo_surface_t *surface;
cairo_t *cr;
gint p_stride, p_n_channels, s_stride;
guchar *p_pixels, *s_pixels;
bool modified = false;
float draw_r = 0.33, draw_g = 0.33, draw_b = 1.0;
bool fill = false;

typedef struct {
  void* prev;
  char* pixbuf_copy;
} history_entry_t;

history_entry_t* history = nil;

// prepare copying between pixbuf and surface
void prepare_blits() {
  g_object_get(G_OBJECT(pixbuf), "width", &width, "height", &height, "rowstride",  &p_stride, "n-channels", &p_n_channels, "pixels",  &p_pixels, NULL );
  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create(surface);
  s_stride = cairo_image_surface_get_stride(surface);
  s_pixels = cairo_image_surface_get_data(surface);
}

// copy from pixbuf to surface
void begin_drawing() {
  int h = height;
  void *p_ptr = p_pixels, *s_ptr = s_pixels;
  /* Copy pixel data from pixbuf to surface */
  while( h-- ) {
    gint i;
    guchar *p_iter = p_ptr;
    guchar *s_iter = s_ptr;
    for( i = 0; i < width; i++ ) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
       /* Pixbuf:  RGB(A)
        * Surface: BGRA */
       if( p_n_channels == 3 ) {
          s_iter[0] = p_iter[2];
          s_iter[1] = p_iter[1];
          s_iter[2] = p_iter[0];
          s_iter[3] = 0xff;
       } else { /* p_n_channels == 4 */
          gdouble alpha_factor = p_iter[3] / (gdouble)0xff;

          s_iter[0] = (guchar)( p_iter[2] * alpha_factor + .5 );
          s_iter[1] = (guchar)( p_iter[1] * alpha_factor + .5 );
          s_iter[2] = (guchar)( p_iter[0] * alpha_factor + .5 );
          s_iter[3] =           p_iter[3];
       }
#elif G_BYTE_ORDER == G_BIG_ENDIAN
       /* Pixbuf:  RGB(A)
        * Surface: ARGB */
       if( p_n_channels == 3 ) {
          s_iter[3] = p_iter[2];
          s_iter[2] = p_iter[1];
          s_iter[1] = p_iter[0];
          s_iter[0] = 0xff;
       } else { /* p_n_channels == 4 */
          gdouble alpha_factor = p_iter[3] / (gdouble)0xff;

          s_iter[3] = (guchar)( p_iter[2] * alpha_factor + .5 );
          s_iter[2] = (guchar)( p_iter[1] * alpha_factor + .5 );
          s_iter[1] = (guchar)( p_iter[0] * alpha_factor + .5 );
          s_iter[0] =           p_iter[3];
       }
#else /* PDP endianness */
       /* Pixbuf:  RGB(A)
        * Surface: RABG */
       if( p_n_channels == 3 ) {
          s_iter[0] = p_iter[0];
          s_iter[1] = 0xff;
          s_iter[2] = p_iter[2];
          s_iter[3] = p_iter[1];
       } else { /* p_n_channels == 4 */
          gdouble alpha_factor = p_iter[3] / (gdouble)0xff;

          s_iter[0] = (guchar)( p_iter[0] * alpha_factor + .5 );
          s_iter[1] =           p_iter[3];
          s_iter[1] = (guchar)( p_iter[2] * alpha_factor + .5 );
          s_iter[2] = (guchar)( p_iter[1] * alpha_factor + .5 );
       }
#endif
       s_iter += 4;
       p_iter += p_n_channels;
    }
    s_ptr += s_stride;
    p_ptr += p_stride;
  }
}

// copy from surface to buffer
void end_drawing() {
  int h = height;
  void *p_ptr = p_pixels, *s_ptr = s_pixels;
  while( h-- ) {
    gint i;
    guchar *p_iter = p_ptr, *s_iter = s_ptr;

    for( i = 0; i < width; i++ ) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
       /* Pixbuf:  RGB(A) * Surface: BGRA */
       gdouble alpha_factor = (gdouble)0xff / s_iter[3];

       p_iter[0] = (guchar)( s_iter[2] * alpha_factor + .5 );
       p_iter[1] = (guchar)( s_iter[1] * alpha_factor + .5 );
       p_iter[2] = (guchar)( s_iter[0] * alpha_factor + .5 );
       if( p_n_channels == 4 )
          p_iter[3] = s_iter[3];
#elif G_BYTE_ORDER == G_BIG_ENDIAN
       /* Pixbuf:  RGB(A) * Surface: ARGB */
       gdouble alpha_factor = (gdouble)0xff / s_iter[0];

       p_iter[0] = (guchar)( s_iter[1] * alpha_factor + .5 );
       p_iter[1] = (guchar)( s_iter[2] * alpha_factor + .5 );
       p_iter[2] = (guchar)( s_iter[3] * alpha_factor + .5 );
       if( p_n_channels == 4 )
          p_iter[3] = s_iter[0];
#else /* PDP endianness */
       /* Pixbuf:  RGB(A) * Surface: RABG */
       gdouble alpha_factor = (gdouble)0xff / s_iter[1];

       p_iter[0] = (guchar)( s_iter[0] * alpha_factor + .5 );
       p_iter[1] = (guchar)( s_iter[3] * alpha_factor + .5 );
       p_iter[2] = (guchar)( s_iter[2] * alpha_factor + .5 );
       if( p_n_channels == 4 )
          p_iter[3] = s_iter[1];
#endif
       s_iter += 4;
       p_iter += p_n_channels;
    }
    s_ptr += s_stride;
    p_ptr += p_stride;
  }
  gtk_widget_queue_draw(image);
  modified = true;
}

void save_pixbuf() {
  printf("save buf\n");
  history_entry_t* entry = malloc(sizeof(history_entry_t));
  entry->pixbuf_copy = malloc(height * p_stride);
  printf("surf h %d stride %d \n", height, p_stride);
  printf("surf buf %p \n", entry->pixbuf_copy);
  /**pixbuf_copy = 1;*/
  /*printf("surf buf %p \n", pixbuf_copy);*/
  memcpy(entry->pixbuf_copy, p_pixels, height * p_stride);
  entry->prev = history;
  history = entry;
}

void revert_pixbuf() {
  printf("revert buf\n");
  //memcpy(p_pixels, pixbuf_copy, height * p_stride);
  if (history) {
    memcpy(p_pixels, history->pixbuf_copy, height * p_stride);
    //history = history->prev;
  }
}

/*static void draw_text() {
    //PangoLayout *layout;
    //layout = pango_cairo_create_layout( cr );
    pango_layout_set_text( layout,
                 gtk_entry_get_text( GTK_ENTRY( data->entry ) ), -1 );
    //pango_cairo_show_layout( cr, layout );
    //g_object_unref( G_OBJECT( layout ) );
}*/

static void draw_rectangle(int x, int y, int xx, int yy) {
  printf("draw rectangle\n");
  begin_drawing();
  //cairo_set_source_rgb(cr, 0.33, 0.33, 1);
  cairo_set_source_rgb(cr, draw_r, draw_g, draw_b);
  cairo_rectangle(cr, x,y,xx-x,yy-y);
  cairo_set_line_width(cr, 3.0);
  if (fill)
    cairo_fill(cr);
  else
    cairo_stroke(cr);
  end_drawing();
}

void quit() {
  gtk_main_quit();
}

void save_and_quit() {
  if (modified)
    cairo_surface_write_to_png(surface, fn);
  gtk_main_quit();
}

bool drawing = false;
int sx,sy;

static void i_mousedown(void* widget, GdkEventButton *event, gpointer data) {
  sx = event->x;
  sy = event->y;
  drawing = true;
  save_pixbuf();
  /*printf("mouse down %d %d \n",sx,sy);*/
}

static void i_mousemove(void* widget, GdkEventButton *event, gpointer data) {
  int x = event->x;
  int y = event->y;
  revert_pixbuf();
  draw_rectangle(sx,sy,x,y);
  /*printf("mouse move %d %d \n",x,y);*/
}

static void i_mouseup(void* widget, GdkEventButton *event, gpointer data) {
  drawing = false;
  int x = event->x;
  int y = event->y;
  /*printf("mouse up %d %d \n",x,y);*/
}

void keypress(GtkWidget* widget, GdkEventKey *event, gpointer data) {
  int key = event->keyval;
  printf("key press %d\n", key);
  if (event->keyval == GDK_KEY_Escape) {
      //quit();
      save_and_quit();
      return TRUE;
  }
  return FALSE;
}

void cb_undo(GtkButton *button, gpointer data) {
  revert_pixbuf();
  begin_drawing();
  end_drawing();
  if (history)
    history = history->prev;
}

void cb_red(GtkButton *button, gpointer data) {
  draw_r = 1.0, draw_g = 0.33, draw_b = 0.33;
}

void cb_green(GtkButton *button, gpointer data) {
  draw_r = 0.33, draw_g = 1.0, draw_b = 0.33;
}

void cb_blue(GtkButton *button, gpointer data) {
  draw_r = 0.33, draw_g = 0.33, draw_b = 1.0;
}

void cb_fill_stroke(GtkButton *button, gpointer data) {
  fill = !fill;
}

void cb_save_and_quit(GtkButton *button, gpointer data) { save_and_quit(); }

int main(int argc, char **argv) {
  printf("%d command line arguments \n", argc);
  for (int i = 0; i < argc; i++)
    printf("%d: %s \n", i, argv[i]);
  if (argc == 1) {
    printf("usage: scrscr filename.png\n");
    return 0;
  }
  fn = argv[1];

  gtk_init( &argc, &argv );

  GError* error = NULL;
  pixbuf = gdk_pixbuf_new_from_file(fn, &error);
  if (pixbuf == NULL) {
      printf("cant open file\n");
      exit(1);
  }

  window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  //gtk_window_move(window, 10, 300);
  gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
  gtk_container_set_border_width(GTK_CONTAINER(window), 10);
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(cb_save_and_quit), NULL);
  g_signal_connect(window, "key-press-event", G_CALLBACK(keypress), NULL);

  vbox = gtk_vbox_new( FALSE, 6 );
  gtk_container_add( GTK_CONTAINER( window ), vbox );

  swindow = gtk_scrolled_window_new( NULL, NULL );
  gtk_box_pack_start( GTK_BOX( vbox ), swindow, TRUE, TRUE, 0 );

  hbox = gtk_hbox_new( TRUE, 6 );
  vbox2 = gtk_vbox_new( TRUE, 6 );

  //pixbuf = gdk_pixbuf_new( GDK_COLORSPACE_RGB, FALSE, 8, 200, 200 );
  //gdk_pixbuf_fill( pixbuf, 0xffff00ff );
  image = gtk_image_new_from_pixbuf( pixbuf );
  //g_object_unref( G_OBJECT( pixbuf ) );

  prepare_blits();

  gtk_window_set_default_size(GTK_WINDOW(window), width+100, height+150);

  gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( swindow ), hbox );

  image_evbox = gtk_event_box_new();
  gtk_container_add(image_evbox, image);

  //gtk_box_pack_start( GTK_BOX( hbox ), image, FALSE, FALSE, 0 );
  gtk_box_pack_start(hbox, vbox2, FALSE, FALSE, 0);
  gtk_box_pack_start(vbox2, image_evbox, FALSE, FALSE, 0);

  g_signal_connect(image_evbox, "button-press-event", G_CALLBACK(i_mousedown), NULL);
  g_signal_connect(image_evbox, "button-release-event", G_CALLBACK(i_mouseup), NULL);
  g_signal_connect(image_evbox, "motion-notify-event", G_CALLBACK(i_mousemove), NULL);

  hbox = gtk_hbox_new( FALSE, 6 );
  gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );

  GtkWidget* button = gtk_button_new_with_label( "undo" );
  g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK(cb_undo), NULL );
  gtk_box_pack_start( GTK_BOX( hbox ), button, FALSE, FALSE, 0 );

  GtkWidget* button_blue = gtk_button_new_with_label( "blue" );
  GtkWidget* button_red = gtk_button_new_with_label( "red" );
  GtkWidget* button_green = gtk_button_new_with_label( "green" );
  //GtkWidget* button_fill = gtk_button_new_with_label( "fill" );
  GtkWidget* button_fill = gtk_toggle_button_new_with_label( "fill" );
  gtk_toggle_button_set_active(button_fill, fill);

  g_signal_connect( G_OBJECT( button_blue ), "clicked", G_CALLBACK(cb_blue), NULL );
  g_signal_connect( G_OBJECT( button_red ), "clicked", G_CALLBACK(cb_red), NULL );
  g_signal_connect( G_OBJECT( button_green ), "clicked", G_CALLBACK(cb_green), NULL );
  g_signal_connect( G_OBJECT( button_fill ), "clicked", G_CALLBACK(cb_fill_stroke), NULL );

  gtk_box_pack_start( GTK_BOX( hbox ), button_blue, FALSE, FALSE, 0 );
  gtk_box_pack_start( GTK_BOX( hbox ), button_red, FALSE, FALSE, 0 );
  gtk_box_pack_start( GTK_BOX( hbox ), button_green, FALSE, FALSE, 0 );
  gtk_box_pack_start( GTK_BOX( hbox ), button_fill, FALSE, FALSE, 0 );

  button = gtk_button_new_with_label( "done" );
  g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK(cb_save_and_quit), NULL );
  gtk_box_pack_end( GTK_BOX( hbox ), button, FALSE, FALSE, 0 );

  gtk_widget_show_all( window );
  gtk_main();
  return 0;
}
