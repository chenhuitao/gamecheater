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

void refresh_mem_treeview(GtkTreeView* treeview, gpointer data)
{
  if (data == NULL) return;
  cheater_t* cheater = (cheater_t*) data;

  GladeXML* xml = cheater->xml;
  treeview = GTK_TREE_VIEW (
      glade_xml_get_widget(xml, "treeview"));

  GtkListStore*  store = GTK_LIST_STORE (gtk_tree_view_get_model(treeview));
  if (store == NULL) return;
  gtk_list_store_clear(store);

  guint i = 0;
  guint addr = GPOINTER_TO_UINT (cheater->addr);
  GtkTreeIter treeiter;
  unsigned char preview[PREVIEW_LENGTH];
  unsigned char buf[PREVIEW_LENGTH * 3];

  while (i < MAX_RESULT_VIEW) {
    if (addr == 0) break;
    addr = addr / PREVIEW_LENGTH * PREVIEW_LENGTH;
    gtk_list_store_append(store, &treeiter);
    gtk_list_store_set(store, &treeiter, ADDRESS_COLUMN, addr, -1);

    get_mem_preview(cheater->pid, addr, preview, sizeof(preview));

    unsigned char t[4];
    int j = 0;
    for (j = 0; j < sizeof(preview); j++) {
      if (3*j >= sizeof(buf)) break;
      snprintf(t, sizeof(t), "%02X ", preview[j]);
      memcpy(buf + j*3, t, 3);
    }
    buf[sizeof(buf)-1] = '\0';

    gtk_list_store_set(store, &treeiter, MEMORY_COLUMN, buf, -1);

    i++;
    addr += PREVIEW_LENGTH;
  }

  return;
}

void renderer_edited(GtkCellRendererText *cellrenderertext,
                 gchar *arg1, gchar *arg2, gpointer data)
{
  if (data == NULL) return;
  cheater_t* cheater = (cheater_t*) data;

  if (strlen(arg2) != PREVIEW_LENGTH *3 -1) return;

  GladeXML* xml = cheater->xml;
  GtkTreeView* treeview = GTK_TREE_VIEW (
      glade_xml_get_widget(xml, "treeview"));

  GtkTreeIter treeiter;
  gchar* tmp = NULL;
  guint addr = 0;

  GtkTreeModel* model = gtk_tree_view_get_model(treeview);
  GtkTreeSelection* selection = gtk_tree_view_get_selection(treeview);

  GList* list = gtk_tree_selection_get_selected_rows(selection, NULL);
  GtkTreePath* path = list->data;

  gtk_tree_model_get_iter(model, &treeiter, path);
  gtk_tree_model_get(model, &treeiter, ADDRESS_COLUMN, &addr, 
      MEMORY_COLUMN, &tmp, -1);

  g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (list);

  if (addr == 0) goto out;
  if (strcmp(tmp, arg2) == 0) goto out;

  int i = 0;
  gint x0 = 0;
  gint x1 = 0;
  unsigned char buf[PREVIEW_LENGTH];
  for (i = 0; i < PREVIEW_LENGTH; i++) {
    x0 = g_ascii_xdigit_value(tmp[i*3 + 0]);
    if (x0 < 0) goto out;
    x1 = g_ascii_xdigit_value(tmp[i*3 + 1]);
    if (x1 < 0) goto out;
    if ((tmp[i*3 + 2] != ' ') && (tmp[i*3 + 2] != '\0')) goto out;
    buf[i] = x0 * 0x10 + x1;
  }

  for (i = 0; i < PREVIEW_LENGTH; i++) {
    x0 = g_ascii_xdigit_value(arg2[i*3 + 0]);
    if (x0 < 0) continue;
    x1 = g_ascii_xdigit_value(arg2[i*3 + 1]);
    if (x1 < 0) continue;
    if ((arg2[i*3 + 2] != ' ') && (arg2[i*3 + 2] != '\0')) break;
    buf[i] = x0 * 0x10 + x1;
  }

  update_value(cheater->pid, (void*) addr, buf, sizeof(buf));
  gtk_list_store_set(GTK_LIST_STORE (model), &treeiter,
      MEMORY_COLUMN, arg2, -1);

out:
  if (tmp != NULL) g_free(tmp);
  refresh_mem_treeview(treeview, cheater);

  return;
}

void init_mem_treeview(GtkTreeView* treeview, cheater_t* cheater) 
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
  g_object_set(G_OBJECT (renderer), "editable", TRUE, NULL);
  /* set font for best view */
//  g_object_set(G_OBJECT (renderer), "font", "MonoSpace", NULL);
  g_signal_connect(G_OBJECT (renderer), "edited", 
                   G_CALLBACK (renderer_edited), cheater);

  column = gtk_tree_view_column_new_with_attributes(_("MEMORY CONTENT"),
                                                    renderer,
                                                    "text", MEMORY_COLUMN,
                                                    NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(treeview, column);

  return;
}

void mem_view_up(GtkTreeView* treeview, gpointer data)
{
  cheater_t* cheater = (cheater_t*) data;

  guint p = GPOINTER_TO_UINT (cheater->addr);
  p -= PREVIEW_LENGTH * MAX_RESULT_VIEW;
  cheater->addr = GUINT_TO_POINTER (p);

  refresh_mem_treeview(treeview, cheater);

  return;
}

void mem_view_down(GtkTreeView* treeview, gpointer data)
{
  cheater_t* cheater = (cheater_t*) data;

  guint p = GPOINTER_TO_UINT (cheater->addr);
  p += PREVIEW_LENGTH * MAX_RESULT_VIEW;
  cheater->addr = GUINT_TO_POINTER (p);

  refresh_mem_treeview(treeview, cheater);

  return;
}

void editor_quit(GtkWidget* widget, gpointer data) 
{
  cheater_t* cheater = (cheater_t*) data;

  g_free(cheater);
}

void create_editor_window(GtkWindow* parent, cheater_t* orig)
{
  if (!GTK_IS_WINDOW (parent) || orig == NULL) return;

  GladeXML* xml = glade_xml_new(GLADE_DIR "/editor.glade", NULL, NULL);
  if (xml == NULL) return;

  cheater_t* cheater = g_malloc(sizeof(cheater_t));
  cheater->pid = orig->pid;
  cheater->value = orig->value;
  cheater->type = orig->type;
  /* not modify orig result */
  cheater->result = NULL;
  cheater->addr = orig->addr;
  /* new widget tree */
  cheater->xml = xml;

  GtkWidget* dialog = glade_xml_get_widget(xml, "window");
  gtk_window_set_transient_for(GTK_WINDOW (dialog), parent);
  gtk_window_set_icon(GTK_WINDOW (dialog), gtk_window_get_icon(parent));
  g_signal_connect(G_OBJECT (dialog), "destroy", 
                   G_CALLBACK (editor_quit), (gpointer) cheater);

  GtkTreeView* treeview = GTK_TREE_VIEW (
      glade_xml_get_widget(xml, "treeview"));

  GtkWidget* button_refresh = glade_xml_get_widget(xml, "button_refresh");
  g_signal_connect(G_OBJECT (button_refresh), "clicked", 
                   G_CALLBACK (refresh_mem_treeview), cheater);

  GtkWidget* button_up = glade_xml_get_widget(xml, "button_up");
  g_signal_connect(G_OBJECT (button_up), "clicked", 
                   G_CALLBACK (mem_view_up), cheater);

  GtkWidget* button_down = glade_xml_get_widget(xml, "button_down");
  g_signal_connect(G_OBJECT (button_down), "clicked", 
                   G_CALLBACK (mem_view_down), cheater);

  init_mem_treeview(treeview, cheater);

  refresh_mem_treeview(treeview, cheater);

  return;
}
