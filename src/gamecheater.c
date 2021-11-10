/***************************************************************************
 *
 *  Gtk+ Game Cheater
 *  Copyright (C) 2005 Alf <h980501427@hotmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify 
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or 
 *  (at your option) any later version. 
 *  
 *  This program is distributed in the hope that it will be useful, 
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *  GNU General Public License for more details. 
 *  
 *  You should have received a copy of the GNU General Public License 
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 **************************************************************************/

#include <string.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "process.h"
#include "search.h"
#include "gamecheater.h"

enum
{
   MYPROC_ID_COLUMN,
   MYPROC_COLUMN,
   N_MYPROC_COLUMNS
};

void replace_activate(GtkTreeView* treeview, gpointer data)
{
  gint i = 0;
  GladeXML* xml = glade_xml_new(GLADE_DIR "/replace.glade", NULL, NULL);
  if (xml == NULL) return;

  GtkWidget* dialog = glade_xml_get_widget(xml, "dialog");

  GladeXML* parent_xml = glade_get_widget_tree(GTK_WIDGET (treeview));
  GtkWindow* parent = GTK_WINDOW (glade_xml_get_widget(parent_xml, "window_main"));
  gtk_window_set_transient_for(GTK_WINDOW (dialog), parent);
  gtk_window_set_icon(GTK_WINDOW (dialog), 
                      gtk_window_get_icon(parent));


  GtkWidget* radio = glade_xml_get_widget(xml, "radio_u32");
  /* default for 32-bit long */
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radio), TRUE);
  gtk_window_set_focus(GTK_WINDOW (dialog), radio);

  radio = glade_xml_get_widget(xml, "radio_u64");
  if (sizeof(long) != sizeof(guint64)) {
    gtk_widget_set_sensitive(radio, FALSE);    
  }

run:
  i = gtk_dialog_run (GTK_DIALOG (dialog));
  switch (i) {
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_APPLY:
    {
      /* get pid */
      GtkTreeIter iter;
      GtkTreeModel* model = gtk_tree_view_get_model(treeview);
      GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);
      GList* list = gtk_tree_selection_get_selected_rows(selection, NULL);
      GtkTreePath* path = list->data;

      unsigned int pid = 0;
      gtk_tree_model_get_iter(model, &iter, path);
      gtk_tree_model_get(model, &iter, MYPROC_ID_COLUMN, &pid, -1);

      g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
      g_list_free (list);

      /* get address */
      guint addr = 0;
      const gchar* text;
      GtkWidget* entry_address = glade_xml_get_widget(xml, "entry_address");
      text = gtk_entry_get_text(GTK_ENTRY (entry_address));
      if (!str2uint(text, &addr)) break;

      /* get value */
      guint value = 0;
      GtkWidget* entry_value = glade_xml_get_widget(xml, "entry_value");
      text = gtk_entry_get_text(GTK_ENTRY (entry_value));
      if (!str2uint(text, &value)) break;

      /* get type */
      unsigned char* value_addr = (unsigned char*) &value;
      guint len = sizeof(guint);
      guint8 u8 = value;
      guint16 u16 = value;
      guint32 u32 = value;
      guint64 u64 = value;
      do {
        radio = glade_xml_get_widget(xml, "radio_u8");
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
          value_addr = (unsigned char*) &u8;
          len = sizeof(guint8);
          break;
        }
        radio = glade_xml_get_widget(xml, "radio_u16");
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
          value_addr = (unsigned char*) &u16;
          len = sizeof(guint16);
          break;
        }
        radio = glade_xml_get_widget(xml, "radio_u32");
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
          value_addr = (unsigned char*) &u32;
          len = sizeof(guint32);
          break;
        }
        radio = glade_xml_get_widget(xml, "radio_u64");
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
          value_addr = (unsigned char*) &u64;
          len = sizeof(guint64);
          break;
        }
      } while (0);
#ifdef DEBUG
printf("update:pid[%u], addr[0x%08X], value[0x%08X], len[%u]\n", 
    pid, addr, update_value, len);
#endif
      update_value(pid, (void*) addr, value_addr, len);
    }
      break;
    default:
      break;
  }
  if (i == GTK_RESPONSE_APPLY) goto run;
  gtk_widget_destroy (dialog);

  return;
}

void init_proc_treeview(GtkTreeView* treeview) 
{
  GtkListStore* myproc_store; 
  GtkCellRenderer* renderer;
  GtkTreeViewColumn* column;

  GtkTreeSelection *select = gtk_tree_view_get_selection(treeview);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

  myproc_store = gtk_list_store_new(N_MYPROC_COLUMNS, 
                                    G_TYPE_INT, G_TYPE_STRING);

  gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(myproc_store)); 
  g_object_unref(myproc_store);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("My process ID"),
                                                    renderer,
                                                    "text", MYPROC_ID_COLUMN,
                                                    NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_clickable(column, TRUE);
  gtk_tree_view_column_set_sort_column_id(column, MYPROC_ID_COLUMN);
  gtk_tree_view_append_column(treeview, column);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("My process name"),
                                                    renderer,
                                                    "text", MYPROC_COLUMN,
                                                    NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_clickable(column, TRUE);
  gtk_tree_view_column_set_sort_column_id(column, MYPROC_COLUMN);
  gtk_tree_view_append_column(treeview, column);

  gtk_tree_view_set_search_column(treeview, MYPROC_COLUMN);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE (myproc_store),
                                       MYPROC_ID_COLUMN, GTK_SORT_DESCENDING);

  return;
}

void refresh_proc_treeview(GtkWidget* widget, gpointer data) 
{
  GtkListStore* myproc_store; 
  GtkTreeIter treeiter;
  struct stat mystat;
  GDir* dproc;
  const gchar* filename;
  guint pid = 0;
  uid_t myuid = getuid();

  myproc_store = GTK_LIST_STORE 
                 (gtk_tree_view_get_model(GTK_TREE_VIEW (widget)));
  if (myproc_store == NULL) return;
  gtk_list_store_clear(myproc_store);

  if (stat("/proc", &mystat) < 0) return;

  dproc = g_dir_open("/proc", 0, NULL);
  if (dproc == NULL) return;

  while ((filename = g_dir_read_name(dproc)) != NULL) {
    if (filename[0] == '.') continue;
    // can not ptrace self
    if (!str2uint(filename, &pid)) continue;
    if (pid == getpid()) continue;
    if (strcmp(filename, "self") == 0) continue;
    char temp[256];
    snprintf(temp, sizeof(temp), "/proc/%s", filename);
    if (stat(temp, &mystat) < 0) continue;
    // skip other user's processes
    if (mystat.st_uid != myuid) continue;
    if (S_ISDIR(mystat.st_mode)) {
      snprintf(temp, sizeof(temp), "/proc/%s/status", filename);
      if (stat(temp, &mystat) < 0) continue;
      if (mystat.st_uid != myuid) continue;
      else {
        FILE* fproc = NULL;
        char buf[1024];

        fproc = fopen(temp, "rb");
        if (fproc == NULL) continue;
        while (fgets(buf, sizeof(buf), fproc) != NULL) {
          buf[strlen(buf) - 1] = '\0';
          /* skip stopped process */
          if (strncmp(buf, "State:\t", 7) == 0) {
            if (buf[7] == 'T') {
              pid = 0;
              break;
            }
          }
          if (strncmp(buf, "Pid:\t", 5) == 0) {
            if (!str2uint(buf+5, &pid)) {
              pid = 0;
              break;
            }
          }
          /* skip traced process */
          if (strncmp(buf, "TracerPid:\t", 11) == 0) {
            if (buf[11] != '0') {
              pid = 0;
              break;
            }
          }
        }
        fclose(fproc);
        if (pid == 0) continue;

        snprintf(temp, sizeof(temp), "/proc/%s/exe", filename);
        int i = readlink(temp, buf, sizeof(buf)-1);
        if (i <= 0) continue;
        buf[i] = '\0';
        for (i = strlen(buf)-1; i >= 0; i--) {
          if (buf[i] == '/') break;
        }
        gtk_list_store_append(myproc_store, &treeiter);
        gtk_list_store_set(myproc_store, &treeiter, MYPROC_ID_COLUMN, pid,
            MYPROC_COLUMN, buf+i+1, -1);
      }
    }
  }

  g_dir_close(dproc);

  return;
}

void treeview_row_activated(GtkTreeView *treeview, GtkTreePath *arg1,
                            GtkTreeViewColumn *arg2, gpointer data)
{
  unsigned int pid;
  GtkTreeIter iter;

  GtkTreeModel* model = gtk_tree_view_get_model(treeview);
  GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);

  /*
    get selected row. only one row can be selected.
    if not, get the first.
  */
  GList* list = gtk_tree_selection_get_selected_rows(selection, NULL);
  GtkTreePath* path = list->data;

  gchar* tmp = NULL;
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_model_get(model, &iter, MYPROC_ID_COLUMN, &pid,
      MYPROC_COLUMN, &tmp, -1);

  g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (list);

  GladeXML* xml = glade_get_widget_tree(GTK_WIDGET (treeview));
  GtkStatusbar* statusbar = GTK_STATUSBAR 
                            (glade_xml_get_widget(xml, "statusbar"));
  GtkWindow* window_main = GTK_WINDOW 
                            (glade_xml_get_widget(xml, "window_main"));

  gtk_statusbar_pop(statusbar, 0);
  if (!ptrace_test(pid)) {
    gchar err[256];
    snprintf(err, sizeof(err), "Ptrace ERROR! pid = %u", pid);
    gtk_statusbar_push(statusbar, 0, err);
    g_free(tmp);
    return;
  }

  create_search_window(window_main, pid, tmp);
  g_free(tmp);

  return;
}

gboolean treeview_button_press(GtkTreeView* treeview, 
                               GdkEventButton* event, gpointer data) 
{
  if (!GTK_IS_MENU (data) || event == NULL) return FALSE;

  GtkMenu *menu = GTK_MENU (data);

  GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);
  GtkTreePath* path = NULL;
  gtk_tree_view_get_path_at_pos(treeview, (gint) event->x, (gint) event->y,
      &path, NULL, NULL, NULL);
  if (path == NULL) return FALSE;
  gtk_tree_selection_select_path(selection, path);
  gtk_tree_path_free(path);


  if (event->type == GDK_BUTTON_PRESS && event->button == 3) {

    gtk_menu_popup(menu, NULL, NULL, NULL, NULL,
                   event->button, event->time);

    return TRUE;
  }

  return FALSE;
}

void about_dialog(GtkWidget* widget, gpointer data) 
{
  gchar* authors[] = {"Alf <h980501427@hotmail.com>",
                       NULL};

  GtkWindow* parent = GTK_WINDOW (data);

  gtk_show_about_dialog(parent,
      "authors", authors,
      "comments", _("A Game Cheater Program.  Just for fun.\
\nIt use \"ptrace\" system call to search and edit memory"),
      "copyright", "Copyright (c) 2005  h980501427@hotmail.com",
      "logo", gtk_window_get_icon(parent),
      "name", PACKAGE,
      "version", VERSION,
  NULL);

  return;
}

void gamecheater_quit(GtkWidget* widget, gpointer data) 
{
  GladeXML* xml = data;
  GtkWidget* popmenu = glade_xml_get_widget(xml, "popmenu");

  gtk_widget_destroy(popmenu);

  gtk_main_quit();

  return;
}


int main(int argc, char* argv[]) 
{

  GladeXML* xml = NULL;

  /* init threads */
  g_thread_init(NULL);
  gdk_threads_init();

  /* init gtk */
#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (PACKAGE, "UTF-8");
  textdomain (PACKAGE);
#endif

  gtk_set_locale ();
  gtk_init(&argc, &argv);

  GOptionContext *context;
  gboolean show_version = FALSE;
  GOptionEntry opt[] = {
    {"version", 'v', 0, G_OPTION_ARG_NONE, &show_version,
     "display the current version and exit.", NULL},
    {NULL}
  };

  context = g_option_context_new("- a GTK+ Game Cheater program.");
  g_option_context_add_main_entries(context, opt, NULL);
  g_option_context_parse(context, &argc, &argv, NULL);
  g_option_context_free(context);

  if (show_version) {
    printf("%s %s\n", PACKAGE, VERSION);
    return 0;
  }

  /* load the interface */
  xml = glade_xml_new(GLADE_DIR "/gamecheater.glade", NULL, NULL);
  if (xml == NULL) return -1;

  /* connect the signals in the interface */

  GtkWidget* window_main = glade_xml_get_widget(xml, "window_main");
  gtk_window_set_icon_from_file(GTK_WINDOW (window_main),
      PIXMAPS_DIR "/gamecheater.png", NULL);
  g_signal_connect(G_OBJECT (window_main), "destroy", 
                   G_CALLBACK (gamecheater_quit), xml);

  GtkWidget* treeview_myproc = glade_xml_get_widget(xml, "treeview_myproc");
  init_proc_treeview(GTK_TREE_VIEW (treeview_myproc));
  refresh_proc_treeview(treeview_myproc, NULL);

  GtkWidget* popmenu = glade_xml_get_widget(xml, "popmenu");
  g_signal_connect(G_OBJECT (treeview_myproc), "row-activated",
                   G_CALLBACK(treeview_row_activated), NULL);
  g_signal_connect(G_OBJECT (treeview_myproc), "button_press_event", 
                   G_CALLBACK (treeview_button_press), popmenu);

  GtkWidget* item = glade_xml_get_widget(xml, "search");
  g_signal_connect_swapped(G_OBJECT (item), "activate",
                   G_CALLBACK (treeview_row_activated), treeview_myproc);

  item = glade_xml_get_widget(xml, "update_value");
  g_signal_connect_swapped(G_OBJECT (item), "activate",
                   G_CALLBACK (replace_activate), treeview_myproc);

  item = glade_xml_get_widget(xml, "refresh");
  g_signal_connect_swapped(G_OBJECT (item), "activate",
                   G_CALLBACK (refresh_proc_treeview), treeview_myproc);

  GtkWidget* quit = glade_xml_get_widget(xml, "quit");
  g_signal_connect(G_OBJECT (quit), "activate",
                   G_CALLBACK (gamecheater_quit), NULL);

  GtkWidget* refresh = glade_xml_get_widget(xml, "refresh");
  g_signal_connect_swapped(G_OBJECT (refresh), "activate",
                   G_CALLBACK (refresh_proc_treeview), treeview_myproc);

  GtkWidget* about = glade_xml_get_widget(xml, "about");
  g_signal_connect(G_OBJECT (about), "activate",
                   G_CALLBACK (about_dialog), (gpointer) window_main);

  /* enter the GTK main loop */
  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  return 0;
}
