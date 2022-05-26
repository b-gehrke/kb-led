// gcc -o kb-led kb-led.c `pkg-config --cflags --libs gtk4` -pthread

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>

#include <pthread.h>

static GtkWidget *btn_caps, *btn_num, *btn_scr;
static int fd = 0;
static int leds = 0;
static int led_cap = LED_CAP;
static int led_num = LED_NUM;
static int led_scr = LED_SCR;

static void
toggle_led(GtkWidget *widget, gpointer data)
{
  int led, r;
  led = *(int*)data;
  r = ioctl(fd, KDSETLED, leds & led ? leds ^ led : leds | led);

  if (r < 0)
  {
    g_print("Error setting led (%d): %s\n", errno, strerror(errno));
  }
}

static void
activate(GtkApplication *app, gpointer user_data)
{
  GtkWidget *window;
  GdkDisplay *display;
  GtkCssProvider *provider;
  GtkWidget *box;

  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Window");
  gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);

  box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_window_set_child(GTK_WINDOW(window), box);

  btn_caps = gtk_button_new_with_label("CAPS");
  g_signal_connect(btn_caps, "clicked", G_CALLBACK(toggle_led), (void *)&led_cap);
  gtk_box_append(GTK_BOX(box), btn_caps);

  btn_num = gtk_button_new_with_label("NUM");
  g_signal_connect(btn_num, "clicked", G_CALLBACK(toggle_led), (void *)&led_num);
  gtk_box_append(GTK_BOX(box), btn_num);

  btn_scr = gtk_button_new_with_label("SCROLL");
  g_signal_connect(btn_scr, "clicked", G_CALLBACK(toggle_led), (void *)&led_scr);
  gtk_box_append(GTK_BOX(box), btn_scr);

  display = gdk_display_get_default();
  provider = gtk_css_provider_new();

  char* css = ".green{background:green;}";
  gtk_css_provider_load_from_data(provider, css, strlen(css));
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

  gtk_window_present(GTK_WINDOW(window));
}

static void *
poll_led(void *_)
{
  int r, tmp;
  GtkStyleContext *sc_caps, *sc_num, *sc_scr;

  sleep(1);

  sc_caps = gtk_widget_get_style_context(btn_caps);
  sc_num = gtk_widget_get_style_context(btn_num);
  sc_scr = gtk_widget_get_style_context(btn_scr);
  while (1)
  {
    r = ioctl(fd, KDGETLED, &tmp);
    if (r < 0)
    {
      g_print("Error polling led (%d): %s\n", errno, strerror(errno));
    }
    else
    {
      if (tmp != leds)
      {
        leds = tmp;
        if (leds & LED_CAP)
        {
          gtk_style_context_add_class(sc_caps, "green");
        }
        else
        {
          gtk_style_context_remove_class(sc_caps, "green");
        }

        if (leds & LED_NUM)
        {
          gtk_style_context_add_class(sc_num, "green");
        }
        else
        {
          gtk_style_context_remove_class(sc_num, "green");
        }

        if (leds & LED_SCR)
        {
          gtk_style_context_add_class(sc_scr, "green");
        }
        else
        {
          gtk_style_context_remove_class(sc_scr, "green");
        }
      }
    }

    sleep(0);
  }

  return NULL;
}

int main(int argc, char **argv)
{
  int status;
  GtkApplication *app;
  pthread_t t;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <path to file>\n", argv[0]);
    exit(1);
  }

  fd = open(argv[1], O_NOCTTY);
  if(fd < 0) {
    fprintf(stderr, "Cannot open file '%s' (%d): %s\n", argv[1], errno, strerror(errno));
    exit(2);
  }

  app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

  pthread_create(&t, NULL, poll_led, NULL);

  status = g_application_run(G_APPLICATION(app), 1, argv);
  g_object_unref(app);

  pthread_cancel(t);
  pthread_join(t, NULL);

  close(fd);

  return status;
}