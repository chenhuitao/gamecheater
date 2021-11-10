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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gamecheater.h"
#include "search.h"
#include "process.h"
#include "editor.h"

#ifndef G_MAXUINT8
#define G_MAXUINT8 0xFF
#endif

void addr_cell_data_func(GtkTreeViewColumn* column, GtkCellRenderer *renderer, 
                         GtkTreeModel *model, GtkTreeIter *iter, 
                         gpointer data)
{
  unsigned long addr = 0;
  char text[32];

  gtk_tree_model_get(model, iter, ADDRESS_COLUMN, &addr, -1);
  snprintf(text, sizeof(text), "0x%0*lX", sizeof(long)*2, addr);
  g_object_set(renderer, "text", text, NULL);

  return;
}

void init_treeview(GtkTreeView* treeview) 
{
  GtkListStore* store; 
  GtkCellRenderer* renderer;
  GtkTreeViewColumn* column;

  GtkTreeSelection *select = gtk_tree_view_get_selection(treeview);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

  store = gtk_list_store_new(N_COLUMNS, G_TYPE_UINT, G_TYPE_STRING);

  gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store)); 
  g_object_unref(store);

  renderer = gtk_cell_renderer_text_new();
//  g_object_set(G_OBJECT (renderer), "font", "MonoSpace", NULL);
  column = gtk_tree_view_column_new_with_attributes(_("MEMORY ADDRESS"),
                                                    renderer,
                                                    "text", ADDRESS_COLUMN,
                                                    NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(treeview, column);
  gtk_tree_view_column_set_cell_data_func(column, renderer,
      (GtkTreeCellDataFunc) addr_cell_data_func, NULL, NULL);


  renderer = gtk_cell_renderer_text_new();
//  g_object_set(G_OBJECT (renderer), "font", "MonoSpace", NULL);
  column = gtk_tree_view_column_new_with_attributes(_("MEMORY PREVIEW"),
                                                    renderer,
                                                    "text", MEMORY_COLUMN,
                                                    NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(treeview, column);

  return;
}

void refresh_treeview(GtkTreeView* treeview, gpointer data) 
{
  if (data == NULL) return;

  cheater_t* cheater = (cheater_t*) data;

  GtkListStore*  store = GTK_LIST_STORE (gtk_tree_view_get_model(treeview));
  if (store == NULL) return;
  gtk_list_store_clear(store);

  if (cheater->result == NULL) return;

  unsigned long i = 0;
  void* p = NULL;
  GtkTreeIter treeiter;
  unsigned char preview[PREVIEW_LENGTH];
  unsigned char buf[PREVIEW_LENGTH * 3];

  while (i < MAX_RESULT_VIEW) {
    p = g_slist_nth_data(cheater->result, i);
    if (p == NULL) break;

    gtk_list_store_append(store, &treeiter);
    gtk_list_store_set(store, &treeiter, ADDRESS_COLUMN, 
        (unsigned long) p, -1);

    get_mem_preview(cheater->pid, p, preview, sizeof(preview));

    unsigned char t[4];
    unsigned long j = 0;
    for (j = 0; j < sizeof(preview); j++) {
      if (3*j >= sizeof(buf)) break;
      snprintf(t, sizeof(t), "%02X ", preview[j]);
      memcpy(buf + j*3, t, 3);
    }
    buf[sizeof(buf)-1] = '\0';

    gtk_list_store_set(store, &treeiter, MEMORY_COLUMN, buf, -1);
    i++;
  }

  return;
}

void row_activated(gpointer data, GtkTreePath *arg1,
                   GtkTreeViewColumn *arg2, GtkTreeView *treeview)
{
  if (data == NULL) return;

  gint i = 0;
  GladeXML* xml = glade_xml_new(GLADE_DIR "/input.glade", NULL, NULL);
  if (xml == NULL) return;

  GtkWidget* dialog = glade_xml_get_widget(xml, "dialog");

  cheater_t* cheater = (cheater_t*) data;
  GladeXML* parent_xml = cheater->xml;
  GtkWindow* parent = GTK_WINDOW (glade_xml_get_widget(parent_xml, "window"));
  gtk_window_set_transient_for(GTK_WINDOW (dialog), parent);
  gtk_window_set_icon(GTK_WINDOW (dialog), 
                      gtk_window_get_icon(parent));

  treeview = GTK_TREE_VIEW (glade_xml_get_widget(parent_xml, "treeview"));

  i = gtk_dialog_run (GTK_DIALOG (dialog));
  switch (i) {
    case GTK_RESPONSE_OK:
    {
      GtkTreeIter iter;
      unsigned long addr = 0;

      GtkTreeModel* model = gtk_tree_view_get_model(treeview);
      GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);

      GList* list = gtk_tree_selection_get_selected_rows(selection, NULL);
      GtkTreePath* path = list->data;

      gtk_tree_model_get_iter(model, &iter, path);
      gtk_tree_model_get(model, &iter, ADDRESS_COLUMN, &addr, -1);

      g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
      g_list_free (list);

      GtkWidget* entry = glade_xml_get_widget(xml, "entry");
      guint64 value = 0;
      if (!str2u64(gtk_entry_get_text(GTK_ENTRY(entry)), &value)) break;

      void* value_addr = (void*) &value;
      guint len = sizeof(long);
      guint8 u8 = value;
      guint16 u16 = value;
      guint32 u32 = value;
      guint64 u64 = value;
      switch (cheater->type) {
        case TYPE_U8 :
        {
          value_addr = (void*) &u8;
          len = sizeof(guint8);
          break;
        }
        case TYPE_U16 :
        {
          value_addr = (void*) &u16;
          len = sizeof(guint16);
          break;
        }
        case TYPE_U32 :
        {
          value_addr = (void*) &u32;
          len = sizeof(guint32);
          break;
        }
        case TYPE_U64 :
        {
          value_addr = (void*) &u64;
          len = sizeof(guint64);
          break;
        }
        default :
          break;
      } //end of switch()
      update_value(cheater->pid, (void*) addr, value_addr, len);
    }
      break;
    default:
      break;
  }
  gtk_widget_destroy (dialog);

  return;
}

void edit_activate(GtkWidget* widget, gpointer data)
{
  if (data == NULL) return;

  cheater_t* cheater = (cheater_t*) data;

  GladeXML* xml = cheater->xml;
  GtkWindow* parent = GTK_WINDOW (glade_xml_get_widget(xml, "window"));
  GtkTreeView* treeview = GTK_TREE_VIEW (
      glade_xml_get_widget(xml, "treeview"));

  GtkTreeIter iter;
  unsigned long addr = 0;

  GtkTreeModel* model = gtk_tree_view_get_model(treeview);
  GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);

  GList* list = gtk_tree_selection_get_selected_rows(selection, NULL);
  GtkTreePath* path = list->data;

  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_model_get(model, &iter, ADDRESS_COLUMN, &addr, -1);

  g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (list);

  cheater->addr = (void*) addr;
  create_editor_window(parent, cheater);

  return;
}

gboolean treeview_popupmenu(GtkTreeView* treeview, 
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

void entry_activate(GtkEntry* entry, gpointer data)
{
  if (data == NULL) return;

  cheater_t* cheater = (cheater_t*) data;

  /* get type */
  GladeXML* xml = cheater->xml;
  GtkWidget* radio;
  cheater->type = TYPE_AUTO;

  do {
    radio = glade_xml_get_widget(xml, "radiobutton_auto");
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
      cheater->type = TYPE_AUTO;
      break;
    }
    radio = glade_xml_get_widget(xml, "radiobutton_u8");
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
      cheater->type = TYPE_U8;
      break;
    }
    radio = glade_xml_get_widget(xml, "radiobutton_u16");
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
     cheater->type = TYPE_U16;
      break;
    }
    radio = glade_xml_get_widget(xml, "radiobutton_u32");
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
      cheater->type = TYPE_U32;
      break;
    }
    radio = glade_xml_get_widget(xml, "radiobutton_u64");
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (radio))) {
      cheater->type = TYPE_U64;
      break;
    }
  } while (0);

  /* get value */

  guint64 value = 0;
  if (!str2u64(gtk_entry_get_text(entry), &value)) return;
  /*
    If the value is too big and is out of the uint range, only get the 
    low-digit value, ignore high-digit. If you select search type uint8  
    and input search value - 0x101, because of 0x101 is out of the uint8 
    range, the true search value is 0x01. If you are not sure which type 
    is fit in with the search value, select auto type.
  */
  cheater->value = value;

  if (cheater->type == TYPE_AUTO) {
    do {
      if (value >= 0 && value <= G_MAXUINT8) {
        cheater->type = TYPE_U8;
        break;
      }
      if (value > G_MAXUINT8 && value <= G_MAXUINT16) {
        cheater->type = TYPE_U16;
        break;
      }
      if (value > G_MAXUINT16 && value <= G_MAXUINT32) {
        cheater->type = TYPE_U32;
        break;
      }
//      if (sizeof(long) != sizeof(guint64)) break;
      if (value > G_MAXUINT32) {
        cheater->type = TYPE_U64;
        break;
      }
    } while (0);
  }
  if (cheater->type == TYPE_AUTO) {
    /* the value is too big, use default type */
    cheater->type = TYPE_U32;
    return;
  }

#ifdef DEBUG
printf("start searth thread. type[%u], value[%llu].\n", cheater->type, value);
#endif
//  search_value(cheater);
  if (!g_thread_create((GThreadFunc) search_value, cheater, FALSE, NULL)) {
#ifdef DEBUG
printf("Failed to create thread.\n");
#endif
    return;
  }

  return;
}

void search_quit(GtkWidget* widget, gpointer data) 
{
  cheater_t* cheater = (cheater_t*) data;

  GladeXML* xml = cheater->xml;

  GtkWidget* popmenu = glade_xml_get_widget(xml, "popmenu");

  g_slist_free(cheater->result);
  g_free(cheater);

  gtk_widget_destroy(popmenu);

  return;
}

void create_search_window(GtkWindow* parent, unsigned long pid, gchar* name)
{
  if (!GTK_IS_WINDOW (parent) || pid == 0 || name == NULL) return;

  /* load the interface */
  GladeXML* xml = glade_xml_new(GLADE_DIR "/search.glade", NULL, NULL);
  if (xml == NULL) return;

  /* init cheater */
  cheater_t* cheater = g_malloc(sizeof(cheater_t));
  cheater->pid = pid;
  cheater->value = 0;
  cheater->type = TYPE_AUTO;
  cheater->result = NULL;
  cheater->addr = NULL;
  cheater->xml = xml;

  /* connect signals */
  GtkWidget* window = glade_xml_get_widget(xml, "window");
  gtk_window_set_transient_for(GTK_WINDOW (window), parent);
  gtk_window_set_icon(GTK_WINDOW (window), 
                      gtk_window_get_icon(parent));

  gchar* title = g_strdup_printf("%lu - %s", pid, name);
  gtk_window_set_title(GTK_WINDOW (window), title);
  g_free(title);
  g_signal_connect(G_OBJECT (window), "destroy", 
                   G_CALLBACK (search_quit), (gpointer) cheater);

  GtkWidget* treeview = glade_xml_get_widget(xml, "treeview");
  init_treeview(GTK_TREE_VIEW (treeview));

  GtkWidget* popmenu = glade_xml_get_widget(xml, "popmenu");
  g_signal_connect_swapped(G_OBJECT (treeview), "row-activated",
                   G_CALLBACK(row_activated), (gpointer) cheater);
  g_signal_connect(G_OBJECT (treeview), "button_press_event", 
                   G_CALLBACK (treeview_popupmenu), (gpointer) popmenu);

  GtkWidget* item = glade_xml_get_widget(xml, "new_value");
  g_signal_connect_swapped(G_OBJECT (item), "activate",
                     G_CALLBACK (row_activated), (gpointer) cheater);

  item = glade_xml_get_widget(xml, "edit_memory");
  g_signal_connect(G_OBJECT (item), "activate",
                   G_CALLBACK (edit_activate), (gpointer) cheater);

  GtkWidget* entry = glade_xml_get_widget(xml, "entry");
  g_signal_connect(G_OBJECT (entry), "activate", 
                   G_CALLBACK (entry_activate), (gpointer) cheater);
  /* set default focus */
  gtk_window_set_focus(GTK_WINDOW (window), entry);

  GtkWidget* radio = glade_xml_get_widget(xml, "radiobutton_auto");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radio), TRUE);

  /* 
    PTRACE_PEEKDATA return long type.
    The size is determined by the OS variant.
  */
#ifdef DISABLE_TYPE_U64
  /* Not 64-bit platform */
  if (sizeof(long) != sizeof(guint64)) {
    radio = glade_xml_get_widget(xml, "radiobutton_u64");
    gtk_widget_set_sensitive(radio, FALSE);    
  }
#endif

  return;
}
