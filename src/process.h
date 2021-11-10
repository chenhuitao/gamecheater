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

#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#define MAX_RESULT_VIEW 8
#define PREVIEW_LENGTH 0x10
#define MEMORY_BLOCK_SIZE 0x1000

enum
{
  TYPE_AUTO,
  TYPE_U8,
  TYPE_U16,
  TYPE_U32,
  TYPE_U64,
  TYPE_NUMS
};


typedef struct {
  unsigned long pid; 
  guint64 value;
  unsigned int type;
  /*
     store memory address, updated after every search
     auto mode, ptrace min length byte.
  */
  GSList* result;
  /* edit address */
  gpointer addr;
  /* GUI widget tree */
  GladeXML* xml;
} cheater_t;

#ifdef __cplusplus
extern "C" {
#endif

gboolean str2u64(const char* str, guint64* pu64);

gboolean ptrace_test(unsigned long pid);

void get_mem_preview(unsigned long pid, void* addr,
                     void* preview, unsigned long len);

void* search_value(cheater_t* cheater);

void update_value(unsigned long pid, void* addr, 
                  void* value, unsigned long len);

#ifdef __cplusplus
}
#endif

#endif /* _PROCESS_H_ */
