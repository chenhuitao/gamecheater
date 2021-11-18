/***************************************************************************
 *
 *  Gtk+ Game Cheater
 *  Copyright (C) 2005 Alf <h980501427@163.com>
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

/*
#ifndef HAVE_PTRACE
#error "You must have \"ptrace\" systemcall!"
#endif
*/

#include "gamecheater.h"


static GtkListStore* get_store_proc(GtkListStore* store_proc)
{
  GtkListStore* store = store_proc;
  if (store == NULL) {
    store = gtk_list_store_new(N_PROC_COLUMNS, G_TYPE_LONG,
                               G_TYPE_LONG,G_TYPE_STRING);
  } else {
    gtk_list_store_clear(store);
  }

  GtkTreeIter iter;
  struct stat mystat;
  GDir* dproc;
  const gchar* filename;
  glong pid = 0;
  glong uid = 0;

  if (stat("/proc", &mystat) < 0) return store;

  dproc = g_dir_open("/proc", 0, NULL);
  if (dproc == NULL) return store;

  while ((filename = g_dir_read_name(dproc)) != NULL) {
    if (filename[0] == '.') continue;
    /* can not ptrace self */
    pid = strtol(filename, NULL, 0);
    if (pid <= 1) continue;
    if (pid == getpid()) continue;
    if (strcmp(filename, "self") == 0) continue;
    char temp[256];
    snprintf(temp, sizeof(temp), "/proc/%s", filename);
    if (stat(temp, &mystat) < 0) continue;
    if (S_ISDIR(mystat.st_mode)) {
      snprintf(temp, sizeof(temp), "/proc/%s/maps", filename);
      if (stat(temp, &mystat) < 0) continue;
        uid = mystat.st_uid;
        snprintf(temp, sizeof(temp), "/proc/%s/exe", filename);
        char buf[1024];
        int i = readlink(temp, buf, sizeof(buf)-1);
        if (i <= 0) continue;
        buf[i] = '\0';
        for (i = strlen(buf)-1; i >= 0; i--) {
          if (buf[i] == '/') break;
        }
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, PROC_PID_COLUMN, pid,
            PROC_UID_COLUMN, uid, PROC_NAME_COLUMN, buf+i+1, -1);
    }
  }

  g_dir_close(dproc);

  return store;
}

static void init_treeview_proc(GtkTreeView* treeview) 
{
  GtkCellRenderer* renderer;
  GtkTreeViewColumn* column;

  GtkTreeSelection *select = gtk_tree_view_get_selection(treeview);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

  GtkListStore* store = NULL; 
  store = get_store_proc(store);

  gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store)); 
  g_object_unref(store);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("Process ID"),
      renderer, "text", PROC_PID_COLUMN, NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_clickable(column, TRUE);
  gtk_tree_view_column_set_sort_column_id(column, PROC_PID_COLUMN);
  gtk_tree_view_append_column(treeview, column);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("Process Name"),
      renderer, "text", PROC_NAME_COLUMN, NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_clickable(column, TRUE);
  gtk_tree_view_column_set_sort_column_id(column, PROC_NAME_COLUMN);
  gtk_tree_view_append_column(treeview, column);

  gtk_tree_view_set_search_column(treeview, PROC_NAME_COLUMN);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE (store),
                                       PROC_PID_COLUMN, GTK_SORT_DESCENDING);

  GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);
  GtkTreePath* path = gtk_tree_path_new_first();
  gtk_tree_selection_select_path(selection, path);

  return;
}

static gboolean treeview_proc_button_press(GtkTreeView* treeview, 
                                           GdkEventButton* event,
                                           gpointer data) 
{
  if (!GTK_IS_MENU (data) || event == NULL) return FALSE;

  GtkMenu *menu = GTK_MENU (data);

  if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
    GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);
    GtkTreePath* path = NULL;
    gtk_tree_view_get_path_at_pos(treeview, (gint) event->x, (gint) event->y,
        &path, NULL, NULL, NULL);

    if (path == NULL) return FALSE;
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_path_free(path);

//    gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
    gtk_menu_popup_at_pointer(menu, NULL);

    return TRUE;
  }

  return FALSE;
}

static void refresh_treeview_proc(GtkWidget* widget,
                                  gpointer data) 
{
  GtkListStore* store; 

  store = GTK_LIST_STORE (gtk_tree_view_get_model(GTK_TREE_VIEW (widget)));
  if (store == NULL) return;

  store = get_store_proc(store);

  return;
}

static void treeview_proc_row_activated(GtkTreeView *treeview,
                                        GtkTreePath *arg1,
                                        GtkTreeViewColumn *arg2,
                                        gpointer data)
{
  glong pid;
  GtkTreeIter iter;

  GtkTreeModel* model = gtk_tree_view_get_model(treeview);
  GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);

  /*
    get selected row. only one row can be selected.
    if not, get the first.
  */
  GList* list = gtk_tree_selection_get_selected_rows(selection, NULL);
  GtkTreePath* path = list->data;

  gchar* name = NULL;
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_model_get(model, &iter, PROC_PID_COLUMN, &pid,
      PROC_NAME_COLUMN, &name, -1);

  g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (list);

  GtkBuilder* builder = g_object_get_data(G_OBJECT(treeview), "builder");
/*
  GtkStatusbar* statusbar = GTK_STATUSBAR 
                            (gtk_builder_get_object(builder, "statusbar"));
*/
  GtkWindow* window_main = GTK_WINDOW 
                            (gtk_builder_get_object(builder, "window_main"));

/*
  gtk_statusbar_pop(statusbar, 0);
  if (!ptrace_test(pid)) {
    gchar err[256];
    snprintf(err, sizeof(err), "Ptrace ERROR! pid = %lu", pid);
    gtk_statusbar_push(statusbar, 0, err);
    g_free(name);
    return;
  }
*/

  if (gc_ptrace_test((pid_t) pid) != 0) {
    GtkWidget* dlg = gtk_message_dialog_new(window_main,
        GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
        _("PTRACE  %s  ERROR!\nPID[%ld]"), name, pid);
    gtk_dialog_run(GTK_DIALOG (dlg));
    gtk_widget_destroy(dlg);
    g_free(name);
    return;
  }

  create_search_window(window_main, (pid_t) pid, name);

  g_free(name);

  return;
}

static void run_replace_dialog(GtkTreeView* treeview,
                               gpointer data)
{
  gint i = 0;
  GtkBuilder* builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, UI_DIR "/replace.ui", NULL);

  GObject* dialog = gtk_builder_get_object(builder, "dialog");

  GtkBuilder* builder_main = g_object_get_data(G_OBJECT(treeview), "builder");
  GtkWindow* parent = GTK_WINDOW 
      (gtk_builder_get_object(builder_main, "window_main"));
  gtk_window_set_transient_for(GTK_WINDOW (dialog), parent);
  gtk_window_set_icon(GTK_WINDOW (dialog), 
                      gtk_window_get_icon(parent));

  GObject* radio = gtk_builder_get_object(builder, "radio_u32");
  /* default for 32-bit long */
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radio), TRUE);
  gtk_window_set_focus(GTK_WINDOW (dialog), GTK_WIDGET (radio));

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

      long pid = 0;
      gtk_tree_model_get_iter(model, &iter, path);
      gtk_tree_model_get(model, &iter, PROC_PID_COLUMN, &pid, -1);

      g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
      g_list_free (list);

      /* get address */
      unsigned long addr = 0;
      const gchar* text;

      GObject* entry_address = gtk_builder_get_object(builder, "entry_address");
      text = gtk_entry_get_text(GTK_ENTRY (entry_address));

      errno = 0;
      addr = strtoul(text, NULL, 0);
      if (errno != 0) {
        GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW (dialog),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            _("THE NUMBER IS ILLEGAL!\nNUMBER[%s]"), text);
        gtk_dialog_run(GTK_DIALOG (dlg));
        gtk_widget_destroy(dlg);
        goto run;
      }

      /* get value */
      guint64 value = 0;
      GObject* entry_value = gtk_builder_get_object(builder, "entry_value");
      text = gtk_entry_get_text(GTK_ENTRY (entry_value));
      errno = 0;
      value = strtoll(text, NULL, 0);
      if (errno != 0) {
        GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW (dialog),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            _("THE NUMBER IS ILLEGAL!\nNUMBER[%s]"), text);
        gtk_dialog_run(GTK_DIALOG (dlg));
        gtk_widget_destroy(dlg);
        goto run;
      }

      /* get type */
      void* value_addr = (void*) &value;
      unsigned long len = sizeof(long);
      guint8 u8 = value;
      guint16 u16 = value;
      guint32 u32 = value;
      guint64 u64 = value;
      do {
        radio = gtk_builder_get_object(builder, "radio_u8");
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
          value_addr = (void*) &u8;
          len = sizeof(guint8);
          break;
        }
        radio = gtk_builder_get_object(builder, "radio_u16");
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
          value_addr = (void*) &u16;
          len = sizeof(guint16);
          break;
        }
        radio = gtk_builder_get_object(builder, "radio_u32");
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
          value_addr = (void*) &u32;
          len = sizeof(guint32);
          break;
        }
        radio = gtk_builder_get_object(builder, "radio_u64");
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
          value_addr = (void*) &u64;
          len = sizeof(guint64);
          break;
        }
      } while (0);

      if (gc_ptrace_stop(pid) != 0) {
        g_warning("ptrace %ld error!\n", pid);
      }
      if (!gc_set_memory(pid, (void*) addr, value_addr, len) != 0) {
        g_warning("set memory addr[0x%0*lX] len[%lu] error!\n",
            (int)sizeof(unsigned long)*2, addr, len);
      }
      gc_ptrace_continue(pid);

    }
      break;
    default:
      break;
  }

  if (i == GTK_RESPONSE_APPLY) goto run;

  gtk_widget_destroy (GTK_WIDGET (dialog));

  return;
}

static void about_dialog(GtkWidget* widget,
                         gpointer data) 
{
  gchar* authors[] = {"Alf <h980501427@163.com>",
                       NULL};

  GtkWindow* parent = GTK_WINDOW (data);

  gtk_show_about_dialog(parent,
      "authors", authors,
      "comments", _("A Game Cheater Program.\
\nIt use \"ptrace\" system call to search and edit memory"),
      "copyright", "Copyright (c) 2005  h980501427@163.com",
      "logo", gtk_window_get_icon(parent),
      "name", PACKAGE,
      "version", VERSION,
  NULL);

  return;
}

static void gamecheater_quit(GtkWidget* widget,
                             gpointer data) 
{
  /* clean objects */

  gtk_main_quit();

  return;
}


int main(int argc, char** argv) 
{

  GtkBuilder* builder = NULL;

  /* init gtk */
#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (PACKAGE, "UTF-8");
  textdomain (PACKAGE);
#endif

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
    g_print("%s %s\n", PACKAGE, VERSION);
    return 0;
  }

  /* load the interface */
  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, UI_DIR "/gamecheater.ui", NULL);

  /* connect the signals in the interface */

  GObject* window_main = gtk_builder_get_object(builder, "window_main");
  gtk_window_set_icon_from_file(GTK_WINDOW (window_main),
      PIXMAPS_DIR "/gamecheater.png", NULL);
  g_signal_connect(G_OBJECT (window_main), "destroy", 
                   G_CALLBACK (gamecheater_quit), builder);

  GObject* treeview_proc = gtk_builder_get_object(builder, "treeview_proc");
  g_object_set_data(G_OBJECT (treeview_proc), "builder", builder);
  init_treeview_proc(GTK_TREE_VIEW (treeview_proc));


  GObject* popmenu = gtk_builder_get_object(builder, "popmenu");
  g_signal_connect(G_OBJECT (treeview_proc), "row-activated",
                   G_CALLBACK(treeview_proc_row_activated), NULL);
  g_signal_connect(G_OBJECT (treeview_proc), "button_press_event", 
                   G_CALLBACK (treeview_proc_button_press), popmenu);

  GObject* item = NULL;
  item = gtk_builder_get_object(builder, "search");
  g_signal_connect_swapped(G_OBJECT (item), "activate",
                   G_CALLBACK (treeview_proc_row_activated), treeview_proc);

  item = gtk_builder_get_object(builder, "update_value");
  g_signal_connect_swapped(G_OBJECT (item), "activate",
                   G_CALLBACK (run_replace_dialog), treeview_proc);

  item = gtk_builder_get_object(builder, "refresh_proc");
  g_signal_connect_swapped(G_OBJECT (item), "activate",
                   G_CALLBACK (refresh_treeview_proc), treeview_proc);

  GObject* quit = gtk_builder_get_object(builder, "quit");
  g_signal_connect(G_OBJECT (quit), "activate",
                   G_CALLBACK (gamecheater_quit), NULL);

  GObject* refresh = gtk_builder_get_object(builder, "refresh");
  g_signal_connect_swapped(G_OBJECT (refresh), "activate",
                   G_CALLBACK (refresh_treeview_proc), treeview_proc);

  GObject* about = gtk_builder_get_object(builder, "about");
  g_signal_connect(G_OBJECT (about), "activate",
                   G_CALLBACK (about_dialog), (gpointer) window_main);

  /* enter the GTK main loop */
  gtk_main();

  return 0;
}
