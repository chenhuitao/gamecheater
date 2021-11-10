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

#ifndef G_MAXUINT8
#define G_MAXUINT8 0xFF
#endif

typedef struct {
  pid_t pid;
  guint64 value;
  value_type type;
  GSList* result;
  /* GUI widget tree */
  GladeXML* xml;
} cheater_t;

static void addr_cell_func(GtkTreeViewColumn* column, 
                           GtkCellRenderer* renderer, 
                           GtkTreeModel* model,
                           GtkTreeIter* iter, 
                           gpointer data)
{
  unsigned long addr = 0;
  gchar* text = NULL;

  gtk_tree_model_get(model, iter, RESULT_ADDRESS_COLUMN, &addr, -1);
  text = g_strdup_printf("0x%0*lX", sizeof(unsigned long)*2, addr);
  g_object_set(renderer, "text", text, NULL);
  g_free(text);

  return;
}

static GtkListStore* get_store_map(GtkListStore* store_map,
                                   pid_t pid)
{
  GtkListStore* store = store_map;
  if (store == NULL) {
    store = gtk_list_store_new(N_MAP_COLUMNS, G_TYPE_BOOLEAN,
                               G_TYPE_STRING, G_TYPE_LONG,
                               G_TYPE_ULONG, G_TYPE_ULONG);
  } else {
    gtk_list_store_clear(store);
  }

  GtkTreeIter iter;
  FILE* fmaps = NULL;
  char buf[1024];
  char fname[1024];
  gboolean check = FALSE;
  long process = 0;
  int len = 0;

  snprintf(buf, sizeof(buf), "/proc/%ld/maps", (long) pid);
  fmaps = fopen(buf, "rb");
  if (fmaps == NULL) return store;

  while (fgets(buf, sizeof(buf), fmaps) != NULL) {
    unsigned long begin, end, offset, inode;
    int dev_major, dev_minor;
    char perms[8];
    len = sscanf(buf, "%lx-%lx %s %lx %d:%d %lu %s", 
      &begin, &end, perms, &offset, &dev_major, &dev_minor, &inode, fname);
    if (len <= 0) g_warning("sscanf function is dangerous! return %d\n", len);
    /* skip read only mem */
    if (perms[1] != 'w') continue;
    check = TRUE;
    len = strlen(fname);
    /* skip device mem */
    if (strncmp(fname, "/dev/", 5) == 0) check = FALSE;
    /* skip system library mem */
    if (strncmp(fname, "/lib/", 5) == 0) check = FALSE;
    if (strncmp(fname, "/usr/X11R6/", 11) == 0) check = FALSE;
    /* skip system library mem */
    if (strncmp(fname, "/usr/lib/", 9) == 0) check = FALSE;
    /* skip font mem */
    if (strncmp(fname + len - 4, ".ttf", 4) == 0) check = FALSE;
    /* skip mo mem */
    if (strncmp(fname + len - 3, ".mo", 3) == 0) check = FALSE;

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                    MAP_CHECK_COLUMN, check,
                    MAP_NAME_COLUMN, fname,
                    MAP_PROCESS_COLUMN, process,
                    MAP_ADDRESS_START_COLUMN, begin,
                    MAP_ADDRESS_END_COLUMN, end,
                    -1);
  }

  if (fmaps != NULL) fclose(fmaps);

  return store;
}

static GtkListStore* get_store_result(GtkListStore* store_result,
                                      cheater_t* cheater)
{
  GtkListStore* store = store_result;
  if (store == NULL) {
    store = gtk_list_store_new(N_RESULT_COLUMNS, G_TYPE_ULONG,
                               G_TYPE_STRING);
  } else {
    gtk_list_store_clear(store);
  }

  if (cheater == NULL) return store;

  if (cheater->result == NULL) return store;

  long i = 0;
  void* p = NULL;
  GtkTreeIter iter;
  unsigned char preview[PREVIEW_LENGTH];
  unsigned char buf[PREVIEW_LENGTH * 3];

  while (i < MAX_RESULT_VIEW) {
    p = g_slist_nth_data(cheater->result, i);
    if (p == NULL) break;

    if (gc_get_memory(cheater->pid, p, preview, sizeof(preview)) != 0) {
      g_warning("get memory addr[0x%0*lX] len[%u] error!\n",
          sizeof(unsigned long)*2, (unsigned long) p, sizeof(preview));
    }
    unsigned char t[4];
    long j = 0;
    for (j = 0; j < sizeof(preview); j++) {
      if (3*j >= sizeof(buf)) break;
      snprintf((char*) t, sizeof(t), "%02X ", preview[j]);
      memcpy(buf + j*3, t, 3);
    }
    buf[sizeof(buf)-1] = '\0';

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, RESULT_ADDRESS_COLUMN, 
        (unsigned long) p, RESULT_MEMORY_COLUMN, buf, -1);
    i++;
  }

  return store;
}

static void* search_memory(cheater_t* cheater)
{
  if (cheater == NULL) return NULL;
  if (cheater->xml == NULL) return NULL;

  GladeXML* xml = cheater->xml;
/*
  GtkWidget* window = glade_xml_get_widget(xml, "window");
*/

  /* stop the process. */
  pid_t pid = cheater->pid;
  if (gc_ptrace_stop(pid) != 0) {
    g_warning("ptrace %ld error!\n", (long) pid);
    return NULL;
  }

  GtkWidget* treeview_lib = glade_xml_get_widget(xml, "treeview_lib");
  GtkWidget* treeview_result = glade_xml_get_widget(xml, "treeview_result");
  GtkWidget* progressbar = glade_xml_get_widget(xml, "progressbar");
  GtkWidget* entry = glade_xml_get_widget(xml, "entry");

  gtk_widget_set_sensitive(entry, FALSE);
  gtk_widget_set_sensitive(treeview_lib, FALSE);

  char buf[MEMORY_BLOCK_SIZE];
  void* addr = NULL;
  unsigned long len = 0;
  unsigned long per = 0;
  long i = 0;
  unsigned long j = 0;
  unsigned long k = 0;

  /* No results. */
  if (cheater->result == NULL) {

    GtkTreeModel* model = gtk_tree_view_get_model
        (GTK_TREE_VIEW (treeview_lib));
    GtkTreeIter iter;
    gboolean valid = FALSE;
    gboolean check = FALSE;
    unsigned long begin = 0;
    unsigned long end = 0;
    double pro_total = 0.0;
    double pro_searched = 0.0;

    valid = gtk_tree_model_get_iter_first (model, &iter);
    while (valid) {
      check = FALSE;
      gtk_tree_model_get (model, &iter, MAP_CHECK_COLUMN, &check,
          MAP_ADDRESS_START_COLUMN, &begin, 
          MAP_ADDRESS_END_COLUMN, &end, -1);
      if (check) pro_total = pro_total + (end - begin);

      gtk_list_store_set(GTK_LIST_STORE (model), 
          &iter, MAP_PROCESS_COLUMN, 0, -1);

      valid = gtk_tree_model_iter_next (model, &iter);
    } /* end of while (valid) */

    valid = gtk_tree_model_get_iter_first (model, &iter);
    while (valid) {
      i = j = k = 0;
      check = FALSE;

      gtk_tree_model_get (model, &iter, MAP_CHECK_COLUMN, &check,
          MAP_ADDRESS_START_COLUMN, &begin, 
          MAP_ADDRESS_END_COLUMN, &end, -1);

      if ((!check) || (end <= begin)) {
        valid = gtk_tree_model_iter_next (model, &iter);
        continue;
      }

      addr = (void*) begin;
      per = (end - begin) / 100;
      while ((unsigned long)addr < end) {

        while (gtk_events_pending ())
          gtk_main_iteration ();
        if (model == NULL || !GTK_IS_LIST_STORE (model)) goto out;

        if ((unsigned long) addr + MEMORY_BLOCK_SIZE <= end)
          len = MEMORY_BLOCK_SIZE;
        else len = end - (unsigned long) addr;
        if (gc_get_memory(pid, addr, (void*)buf, len) != 0) {
          g_warning("get memory addr[0x%0*lX] len[%lu] error!\n",
              sizeof(unsigned long)*2, (unsigned long) addr, len);
          goto out;
        }
        switch (cheater->type) {
          case TYPE_U8 :
          {
            guint8* pu8 = (guint8*) buf;
            guint8 u8 = (guint8) cheater->value;
            for (i = 0; i < len / sizeof(guint8); i++) {
              if (pu8[i] == u8) {
                cheater->result = g_slist_append(cheater->result, 
                    addr + sizeof(guint8) * i);
              }
            }
            break;
          }
          case TYPE_U16 :
          {
            guint16* pu16 = (guint16*) buf;
            guint16 u16 = (guint16) cheater->value;
            for (i = 0; i < len / sizeof(guint16); i++) {
              if (pu16[i] == u16) {
                cheater->result = g_slist_append(cheater->result, 
                    addr + sizeof(guint16) * i);
              }
            }
            break;
          }
          case TYPE_U32 :
          {
            guint32* pu32 = (guint32*) buf;
            guint32 u32 = (guint32) cheater->value;
            for (i = 0; i < len / sizeof(guint32); i++) {
              if (pu32[i] == u32) {
                cheater->result = g_slist_append(cheater->result, 
                    addr + sizeof(guint32) * i);
              }
            }
            break;
          }
          case TYPE_U64 :
          {
            guint64* pu64 = (guint64*) buf;
            guint64 u64 = (guint64) cheater->value;
            for (i = 0; i < len / sizeof(guint64); i++) {
              if (pu64[i] == u64) {
                cheater->result = g_slist_append(cheater->result, 
                    addr + sizeof(guint64) * i);
              }
            }
            break;
          }
          default :
            break;
        } /* end of switch(cheater->type) */

        addr += len;
        pro_searched += len;

        k = ((unsigned long)addr - begin) / per;
        if ( k > j) {
          j = k;
          if (j > 100) j = 100;
          /* update progressbar */
          if (model == NULL || !GTK_IS_LIST_STORE (model)) goto out;
          gtk_list_store_set(GTK_LIST_STORE (model), 
              &iter, MAP_PROCESS_COLUMN, j, -1);
          gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (progressbar),
              pro_searched / pro_total);
        }

      } /* end of while ((unsigned long)addr < end) */

      if (j != 100) {
        gtk_list_store_set(GTK_LIST_STORE (model), 
              &iter, MAP_PROCESS_COLUMN, 100, -1);
      }

      valid = gtk_tree_model_iter_next (model, &iter);
    } /* end of while (valid) */

  }

  else {

    GSList* list = cheater->result;
    k = 0;
    len = g_slist_length(cheater->result);

    while (1) {

      while (gtk_events_pending ())
        gtk_main_iteration ();

      addr = list->data;
      switch (cheater->type) {
        case TYPE_U8 :
        {
          guint8 u8 = (guint8) cheater->value;
          guint8 tu8 = 0;
          if (gc_get_memory(pid, addr, (void*)&tu8, sizeof(guint8)) != 0) {
            g_warning("get memory addr[0x%0*lX] error!\n", 
                sizeof(unsigned long)*2, (unsigned long) addr);
            goto out;
          }
          if (u8 != tu8) {
            list->data = NULL;
          }
          break;
        }
        case TYPE_U16 :
        {
          guint16 u16 = (guint16) cheater->value;
          guint16 tu16 = 0;
          if (gc_get_memory(pid, addr, (void*)&tu16, sizeof(guint16)) != 0) {
            g_warning("get memory addr[0x%0*lX] error!\n", 
                sizeof(unsigned long)*2, (unsigned long) addr);
            goto out;
          }
          if (u16 != tu16) {
            list->data = NULL;
          }
          break;
        }
        case TYPE_U32 :
        {
          guint32 u32 = (guint32) cheater->value;
          guint32 tu32 = 0;
          if (gc_get_memory(pid, addr, (void*)&tu32, sizeof(guint32)) != 0) {
            g_warning("get memory addr[0x%0*lX] error!\n", 
                sizeof(unsigned long)*2, (unsigned long) addr);
            goto out;
          }
          if (u32 != tu32) {
            list->data = NULL;
          }
          break;
        }
        case TYPE_U64 :
        {
          guint64 u64 = (guint64) cheater->value;
          guint64 tu64 = 0;
          if (gc_get_memory(pid, addr, (void*)&tu64, sizeof(guint64)) != 0) {
            g_warning("get memory addr[0x%0*lX] error!\n", 
                sizeof(unsigned long)*2, (unsigned long) addr);
            goto out;
          }
          if (u64 != tu64) {
            list->data = NULL;
          }
          break;
        }
        default :
          break;
      } /* end of switch(cheater->type) */

      list = list->next;
      if (list == NULL) break;

      k++;

      if (k % MEMORY_BLOCK_SIZE == 0) {
        if (GTK_IS_PROGRESS_BAR (progressbar)) goto out;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (progressbar), 
            (double) k / len);
      }

    } /* end of while (1) */

    cheater->result = g_slist_remove_all(cheater->result, NULL);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (progressbar), 0.0);

  }

  GtkListStore* store = GTK_LIST_STORE 
      (gtk_tree_view_get_model(GTK_TREE_VIEW (treeview_result)));
  store = get_store_result(store, cheater);

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (progressbar), 0.0);
  snprintf(buf, sizeof(buf), _("%u matched"), g_slist_length(cheater->result));
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR (progressbar), buf);

  gtk_widget_set_sensitive(treeview_lib, TRUE);
  gtk_widget_set_sensitive(entry, TRUE);

out:
  gc_ptrace_continue(pid);
  return NULL;
}

static void map_check_clicked(GtkCellRendererToggle* cell,
                              gchar* path_str,
                              gpointer data)
{
    GtkTreeIter iter;
    GtkListStore* store = GTK_LIST_STORE (data);
    GtkTreeModel* model = GTK_TREE_MODEL (data);

    GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
    gboolean check;
         
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get (model, &iter, MAP_CHECK_COLUMN, &check, -1);
         
    gtk_tree_path_free (path);

    if (check) {
      gtk_list_store_set(store, &iter, 
          MAP_CHECK_COLUMN, FALSE, MAP_PROCESS_COLUMN, 0,
          MAP_PROCESS_COLUMN, 0, -1);
    } else {
      gtk_list_store_set(store, &iter, 
          MAP_CHECK_COLUMN, TRUE, MAP_PROCESS_COLUMN, 0,
          MAP_PROCESS_COLUMN, 0, -1);
    }

   return;
}

static void init_treeview_lib(GtkTreeView* treeview,
                              pid_t pid) 
{
  GtkCellRenderer* renderer;
  GtkTreeViewColumn* column;

  GtkTreeSelection* select = gtk_tree_view_get_selection(treeview);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

  GtkListStore* store = NULL;
  store = get_store_map(store, pid);

  gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store)); 
  g_object_unref(store);

  renderer = gtk_cell_renderer_toggle_new();
/*
  g_object_set(G_OBJECT (renderer), "font", "MonoSpace", NULL);
*/
  column = gtk_tree_view_column_new_with_attributes(_("SEARCH"),
      renderer, "active", MAP_CHECK_COLUMN, NULL);

  g_object_set(renderer, "activatable", TRUE, NULL);
  g_signal_connect (G_OBJECT(renderer), "toggled", G_CALLBACK
      (map_check_clicked), store);

  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(treeview, column);


  renderer = gtk_cell_renderer_progress_new();
/*
  g_object_set(G_OBJECT (renderer), "font", "MonoSpace", NULL);
*/
  column = gtk_tree_view_column_new_with_attributes(_("LIBRARY NAME"),
      renderer,"text",MAP_NAME_COLUMN, "value", MAP_PROCESS_COLUMN, NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(treeview, column);

  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE (store),
                                       MAP_CHECK_COLUMN, GTK_SORT_DESCENDING);
  return;
}

static void init_treeview_result(GtkTreeView* treeview) 
{
  GtkCellRenderer* renderer;
  GtkTreeViewColumn* column;

  GtkTreeSelection* select = gtk_tree_view_get_selection(treeview);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

  GtkListStore* store = NULL;
  store = get_store_result(store, NULL);

  gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store)); 
  g_object_unref(store);

  renderer = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT (renderer), "font", "MonoSpace", NULL);
  column = gtk_tree_view_column_new_with_attributes(_("MEMORY ADDRESS"),
      renderer, "text", RESULT_ADDRESS_COLUMN, NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(treeview, column);
  gtk_tree_view_column_set_cell_data_func(column, renderer,
      (GtkTreeCellDataFunc) addr_cell_func, NULL, NULL);


  renderer = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT (renderer), "font", "MonoSpace", NULL);
  column = gtk_tree_view_column_new_with_attributes(_("MEMORY PREVIEW"),
      renderer, "text", RESULT_MEMORY_COLUMN, NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(treeview, column);

  return;
}

static void treeview_result_row_activated(gpointer data, 
                                          GtkTreePath* arg1,
                                          GtkTreeViewColumn* arg2,
                                          GtkTreeView* tree)
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

  GtkTreeView* treeview = GTK_TREE_VIEW 
      (glade_xml_get_widget(parent_xml, "treeview_result"));

run:
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
      gtk_tree_model_get(model, &iter, RESULT_ADDRESS_COLUMN, &addr, -1);

      g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
      g_list_free (list);

      const gchar* text = NULL;
      guint64 value = 0;

      GtkWidget* entry = glade_xml_get_widget(xml, "entry");
      errno = 0;
      text = gtk_entry_get_text(GTK_ENTRY (entry));
      value = strtoull(text, NULL, 0);
      if (errno != 0) {
        GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW (dialog),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            _("THE NUMBER IS ILLEGAL!\nNUMBER[%s]"), text);
        gtk_dialog_run(GTK_DIALOG (dlg));
        gtk_widget_destroy(dlg);
        goto run;
      }

      void* value_addr = (void*) &value;
      int len = sizeof(long);
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
      } /* end of switch(cheater->type) */

      if (gc_ptrace_stop(cheater->pid) != 0) {
        g_warning("ptrace %ld error!\n", (long) cheater->pid);
      }
      if (gc_set_memory(cheater->pid, (void*) addr, value_addr, len) != 0) {
        g_warning("set memory addr[0x%0*lX] len[%d] error!\n",
            sizeof(unsigned long)*2, addr, len);
      }
      gc_ptrace_continue(cheater->pid);

    }
      break;
    default:
      break;
  } /* end of switch(i) */

  gtk_widget_destroy(dialog);

  return;
}


static void edit_activate(GtkWidget* widget,
                          gpointer data)
{
  if (data == NULL) return;

  cheater_t* cheater = (cheater_t*) data;

  GladeXML* xml = cheater->xml;
  GtkWindow* parent = GTK_WINDOW (glade_xml_get_widget(xml, "window"));
  GtkTreeView* treeview = GTK_TREE_VIEW (
      glade_xml_get_widget(xml, "treeview_result"));

  GtkTreeIter iter;
  unsigned long addr = 0;

  GtkTreeModel* model = gtk_tree_view_get_model(treeview);
  GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);

  GList* list = gtk_tree_selection_get_selected_rows(selection, NULL);
  GtkTreePath* path = list->data;

  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_model_get(model, &iter, RESULT_ADDRESS_COLUMN, &addr, -1);

  g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (list);

  create_editor_window(parent, cheater->pid, (void*) addr);

  return;
}


static gboolean treeview_result_popupmenu(GtkTreeView* treeview, 
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

    gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
    return TRUE;
  }

  return FALSE;
}

static void expander_notify(GtkExpander* expander,
                           GParamSpec *param_spec,
                           gpointer data)
{
  cheater_t* cheater = (cheater_t*) data;
  if (cheater == NULL) return;

  GladeXML* xml = cheater->xml;
  GtkWidget* label_expander = glade_xml_get_widget(xml, "label_expander");
  GtkWidget* scrolledwindow_lib = 
      glade_xml_get_widget(xml, "scrolledwindow_lib");
  
  if (gtk_expander_get_expanded (expander)) {
    gtk_label_set_text(GTK_LABEL(label_expander), _("Hide memory range"));
    gtk_widget_show(scrolledwindow_lib);
  } else {
    gtk_label_set_text(GTK_LABEL(label_expander), _("Show memory range"));
    gtk_widget_hide(scrolledwindow_lib);
  }

  return;
}

static void entry_activate(GtkEntry* entry,
                           gpointer data)
{
  cheater_t* cheater = (cheater_t*) data;
  if (cheater == NULL) return;

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

  const gchar* text = NULL;
  guint64 value = 0;
  errno = 0;
  text = gtk_entry_get_text(entry);
  value = strtoull(text, NULL, 0);
  if (errno != 0 ) {
    GtkWindow* parent = GTK_WINDOW (glade_xml_get_widget(xml, "window"));
    GtkWidget* dlg = gtk_message_dialog_new(parent,
        GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
    _("THE NUMBER IS ILLEGAL!\nNUMBER[%s]"), text);
    gtk_dialog_run(GTK_DIALOG (dlg));
    gtk_widget_destroy(dlg);
    return;
  }

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

  search_memory(cheater);

/*
  if (!g_thread_create((GThreadFunc) search_memory, cheater, FALSE, NULL)) {
    return;
  }
*/

  return;
}

static void search_quit(GtkWidget* widget,
                        gpointer data) 
{
  cheater_t* cheater = (cheater_t*) data;

  g_slist_free(cheater->result);
  g_free(cheater);

  return;
}

void create_search_window(GtkWindow* parent,
                          pid_t pid,
                          char* name)
{
  if (!GTK_IS_WINDOW (parent) || pid <= 1 || name == NULL) return;

  /* load the interface */
  GladeXML* xml = glade_xml_new(GLADE_DIR "/search.glade", NULL, NULL);
  if (xml == NULL) return;

  /* init cheater */
  cheater_t* cheater = g_malloc(sizeof(cheater_t));
  cheater->pid = (pid_t) pid;
  cheater->value = 0;
  cheater->type = TYPE_AUTO;
  cheater->result = NULL;
  cheater->xml = xml;

  /* connect signals */
  GtkWidget* window = glade_xml_get_widget(xml, "window");
  gtk_window_set_transient_for(GTK_WINDOW (window), parent);
  gtk_window_set_icon(GTK_WINDOW (window), gtk_window_get_icon(parent));

  gchar* title = g_strdup_printf("%ld - %s", (long) pid, name);
  gtk_window_set_title(GTK_WINDOW (window), title);
  g_free(title);
  g_signal_connect(G_OBJECT (window), "destroy", 
                   G_CALLBACK (search_quit), (gpointer) cheater);

  GtkWidget* treeview_lib = glade_xml_get_widget(xml, "treeview_lib");
  init_treeview_lib(GTK_TREE_VIEW (treeview_lib), cheater->pid);

  GtkWidget* treeview_result = glade_xml_get_widget(xml, "treeview_result");
  init_treeview_result(GTK_TREE_VIEW (treeview_result));
  g_signal_connect_swapped(G_OBJECT (treeview_result), "row-activated",
                   G_CALLBACK(treeview_result_row_activated), (gpointer) cheater);

  GtkWidget* popmenu = glade_xml_get_widget(xml, "popmenu");
  g_signal_connect(G_OBJECT (treeview_result), "button_press_event", 
                   G_CALLBACK (treeview_result_popupmenu), (gpointer) popmenu);

  GtkWidget* item = NULL;
  item = glade_xml_get_widget(xml, "new_value");
  g_signal_connect_swapped(G_OBJECT (item), "activate",
                     G_CALLBACK (treeview_result_row_activated), (gpointer) cheater);

  item = glade_xml_get_widget(xml, "edit_memory");
  g_signal_connect(G_OBJECT (item), "activate",
                   G_CALLBACK (edit_activate), (gpointer) cheater);

  GtkWidget* expander = glade_xml_get_widget(xml, "expander");
  g_signal_connect(G_OBJECT (expander), "notify::expanded", 
                   G_CALLBACK (expander_notify), (gpointer) cheater);

  GtkWidget* entry = glade_xml_get_widget(xml, "entry");
  g_signal_connect(G_OBJECT (entry), "activate", 
                   G_CALLBACK (entry_activate), (gpointer) cheater);

  /* set default focus */
  gtk_window_set_focus(GTK_WINDOW (window), entry);

  GtkWidget* radio = glade_xml_get_widget(xml, "radiobutton_auto");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radio), TRUE);

  return;
}
