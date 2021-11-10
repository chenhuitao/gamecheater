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

#ifndef _SEARCH_H_
#define _SEARCH_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

enum
{
  ADDRESS_COLUMN,
  MEMORY_COLUMN,
  N_COLUMNS
};

#ifdef __cplusplus
extern "C" {
#endif

void create_search_window(GtkWindow* parent, unsigned long pid, gchar* name);

void refresh_treeview(GtkTreeView* treeview, gpointer data);

void addr_cell_data_func(GtkTreeViewColumn* column, GtkCellRenderer *renderer, 
                         GtkTreeModel *model, GtkTreeIter *iter, 
                         gpointer data);

#ifdef __cplusplus
}
#endif

#endif /* _SEARCH_H_ */
