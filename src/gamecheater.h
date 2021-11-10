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

#ifndef _GAMECHEATER_H_
#define _GAMECHEATER_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libgcheater.h>

/* Standard gettext macros. */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

enum
{
   PROC_PID_COLUMN,
   PROC_UID_COLUMN,
   PROC_NAME_COLUMN,
   N_PROC_COLUMNS
};

enum
{
   MAP_CHECK_COLUMN,
   MAP_NAME_COLUMN,
   MAP_PROCESS_COLUMN,
   MAP_ADDRESS_START_COLUMN,
   MAP_ADDRESS_END_COLUMN,
   N_MAP_COLUMNS
};


enum
{
  RESULT_ADDRESS_COLUMN,
  RESULT_MEMORY_COLUMN,
  N_RESULT_COLUMNS
};

typedef enum {
  TYPE_AUTO,
  TYPE_U8,
  TYPE_U16,
  TYPE_U32,
  TYPE_U64,
  TYPE_NUMS
} value_type;

#define MAX_RESULT_VIEW 8
#define PREVIEW_LENGTH 0x10
#define MEMORY_BLOCK_SIZE 0x1000

#ifdef __cplusplus
extern "C" {
#endif

void create_search_window(GtkWindow* parent,
                          pid_t pid,
                          char* name);

void create_editor_window(GtkWindow* parent,
                          pid_t pid,
                          void* addr);

#ifdef __cplusplus
}
#endif

#endif /* _GAMECHEATER_H_ */
