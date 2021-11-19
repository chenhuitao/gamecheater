/***************************************************************************
 *
 *  Gtk+ Game Cheater
 *  Copyright (C) 2005 Huitao Chen <h980501427@163.com>
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

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gamecheater.h"

#define ADDRESS_TEXT_LEN  ((sizeof(unsigned long))*2 + 2 + 4)
#define MEMORY_TEXT_LEN (PREVIEW_LENGTH*3)
#define IS_HEXCHAR(ch) ((((ch) >= GDK_KEY_0) && ((ch) <= GDK_KEY_9)) || \
                        (((ch) >= GDK_KEY_A) && ((ch) <= GDK_KEY_F)) || \
                        (((ch) >= GDK_KEY_a) && ((ch) <= GDK_KEY_f)))?1:0
typedef struct {
  pid_t pid;
  void* addr;
  GtkBuilder* builder;
} editor_t;

static gboolean textview_key_press(GtkWidget* widget,
                                    GdkEventKey* event,
                                    gpointer data)
{
  if (data == NULL) return FALSE;
  if (!IS_HEXCHAR(event->keyval)) return FALSE;
  editor_t* editor = (editor_t*) data;
  gchar ch = gdk_keyval_to_upper(event->keyval);
  GtkTextBuffer* buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(widget));
  GtkTextMark* mark = gtk_text_buffer_get_insert(buffer);
  GtkTextIter start;
  GtkTextIter end;

  gint len = ADDRESS_TEXT_LEN + MEMORY_TEXT_LEN;
  gtk_text_buffer_get_iter_at_mark(buffer, &start, mark);
  gint offset = gtk_text_iter_get_offset(&start);
  if (offset % len <  ADDRESS_TEXT_LEN) return FALSE;
  if (((offset % len - ADDRESS_TEXT_LEN) % 3) == 2) return FALSE;

  /* update the memory */
  unsigned long addr = 0;
  unsigned char value = 0;
  gchar* text = NULL;
  gint pos = offset / len * len;
  gtk_text_buffer_get_iter_at_offset(buffer, &start, pos);
  pos += ADDRESS_TEXT_LEN;
  gtk_text_buffer_get_iter_at_offset(buffer, &end, pos);
  text = gtk_text_iter_get_text(&start, &end);

  errno = 0;
  addr = strtoul(text, NULL, 0);
  if (errno != 0) {
    g_warning("get address[%s] error!\n", text);
    g_free(text);
    return FALSE;
  }
  g_free(text);
  addr += (offset % len - ADDRESS_TEXT_LEN) / 3;

  if (((offset % len - ADDRESS_TEXT_LEN) % 3) == 0) {
    pos = offset + 1;
    gtk_text_buffer_get_iter_at_offset(buffer, &start, pos);
    value = (g_ascii_xdigit_value(ch) << 4) + 
        g_ascii_xdigit_value(gtk_text_iter_get_char(&start));
  }
  else {
    pos = offset - 1;
    gtk_text_buffer_get_iter_at_offset(buffer, &start, pos);
    value = (g_ascii_xdigit_value(gtk_text_iter_get_char(&start)) << 4) + 
        g_ascii_xdigit_value(ch);
  }

  if (gc_ptrace_stop(editor->pid) != 0) {
    g_warning("ptrace %ld error!\n", (long) editor->pid);
  }
  if (gc_set_memory(editor->pid, (void*) addr, &value, 
      sizeof(unsigned char)) != 0) {
    g_warning("set memory addr[0x%0*lX] len[%lu] error!\n",
        (int)sizeof(unsigned long)*2, addr, sizeof(unsigned char));
  }
  gc_ptrace_continue(editor->pid);

  /* update the text */
  gtk_text_buffer_get_iter_at_offset(buffer, &start, offset);
  offset += 1;
  gtk_text_buffer_get_iter_at_offset(buffer, &end, offset);
  gtk_text_buffer_delete(buffer, &start, &end);
  gtk_text_buffer_insert(buffer, &start, &ch, 1);

  /* skip the unused char */
  if (((offset % len - ADDRESS_TEXT_LEN) % 3) == 2) {
    offset += 1;
    if (offset % len == 0) offset += ADDRESS_TEXT_LEN;
    gtk_text_buffer_get_iter_at_offset(buffer, &end, offset);
    gtk_text_buffer_place_cursor(buffer, &end);
  }

  return FALSE;
}

static void refresh_textview(GtkTextView* textview, editor_t* editor) 
{
  if (editor == NULL || editor->pid <= 1 || editor->addr == NULL) return;

  gboolean stop = FALSE;
  int i = 0;
  unsigned char* preview = NULL;
  unsigned char* buf = NULL;
  unsigned long addr = ((unsigned long) editor->addr)
      / PREVIEW_LENGTH * PREVIEW_LENGTH;

  preview = g_malloc(PREVIEW_LENGTH * MAX_RESULT_VIEW);
  if (preview == NULL) goto out;
  buf = g_malloc((ADDRESS_TEXT_LEN + MEMORY_TEXT_LEN) * MAX_RESULT_VIEW + 1);
  if (buf == NULL) goto out;

  if (gc_ptrace_stop(editor->pid) != 0) goto out;
  stop = TRUE;

  if (gc_get_memory(editor->pid, (void*) addr, preview, 
      PREVIEW_LENGTH * MAX_RESULT_VIEW) != 0) goto out;

  int j = 0;
  for (i = 0; i < PREVIEW_LENGTH * MAX_RESULT_VIEW; i++) {
    if (i % PREVIEW_LENGTH == 0) {
      sprintf((char*) buf+j, "0x%0*lX    ", (int)sizeof(unsigned long)*2, addr);
      j += ADDRESS_TEXT_LEN;
      addr += PREVIEW_LENGTH;
    }
    if (i % PREVIEW_LENGTH == PREVIEW_LENGTH -1) {
      sprintf((char*) buf+j, "%02X\n", preview[i]);
      j += 3;
    } else {
      sprintf((char*) buf+j, "%02X ", preview[i]);
      j += 3;
    }
  }

  buf[(ADDRESS_TEXT_LEN + MEMORY_TEXT_LEN) * MAX_RESULT_VIEW] = '\0';

  GtkTextBuffer* buffer = gtk_text_view_get_buffer(textview);
  gtk_text_buffer_set_text(buffer, (const gchar*) buf, 
      (ADDRESS_TEXT_LEN + MEMORY_TEXT_LEN) * MAX_RESULT_VIEW);

out:
  if (preview != NULL) g_free(preview);
  if (buf != NULL) g_free(buf);
  if (stop) gc_ptrace_continue(editor->pid);

  return;
}

static void mem_view_refresh(GtkButton* button, gpointer data)
{
  if (data == NULL) return;
  editor_t* editor = (editor_t*) data;

  GObject* textview = gtk_builder_get_object(editor->builder, "textview");
  refresh_textview(GTK_TEXT_VIEW (textview), editor);

  return;
}

static void mem_view_up(GtkButton* button, gpointer data)
{
  if (data == NULL) return;
  editor_t* editor = (editor_t*) data;

  unsigned long p = (unsigned long) editor->addr;
  p -= PREVIEW_LENGTH * MAX_RESULT_VIEW;
  editor->addr = (void*) p;

  GObject* textview = gtk_builder_get_object(editor->builder, "textview");
  refresh_textview(GTK_TEXT_VIEW (textview), editor);

  return;
}

static void mem_view_down(GtkButton* button, gpointer data)
{
  if (data == NULL) return;
  editor_t* editor = (editor_t*) data;

  unsigned long p = (unsigned long) editor->addr;
  p += PREVIEW_LENGTH * MAX_RESULT_VIEW;
  editor->addr = (void*) p;

  GObject* textview = gtk_builder_get_object(editor->builder, "textview");
  refresh_textview(GTK_TEXT_VIEW (textview), editor);

  return;
}

static void editor_quit(GtkWidget* widget,
                        gpointer data) 
{
  editor_t* editor = (editor_t*) data;

  g_free(editor);
}

void create_editor_window(GtkWindow* parent,
                          pid_t pid,
                          void* addr)
{
  if (!GTK_IS_WINDOW (parent) || pid <= 1 || addr == NULL) return;

  GtkBuilder* builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, UI_DIR "/editor.ui", NULL);

  editor_t* editor = g_malloc(sizeof(editor_t));
  editor->pid = pid;
  editor->addr = addr;
  editor->builder = builder;

  GObject* window = gtk_builder_get_object(builder, "window");
  gtk_window_set_transient_for(GTK_WINDOW (window), parent);
  gtk_window_set_icon(GTK_WINDOW (window), gtk_window_get_icon(parent));
  g_signal_connect(G_OBJECT (window), "destroy", 
                   G_CALLBACK (editor_quit), (gpointer) editor);

  GObject* textview = gtk_builder_get_object(builder, "textview");

  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider, "textview { font-family: monospace; }", -1, NULL);
  GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET (textview));
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

  gtk_text_view_set_editable(GTK_TEXT_VIEW (textview), FALSE);
  refresh_textview(GTK_TEXT_VIEW (textview), editor);
  g_signal_connect(G_OBJECT (textview), "key-press-event", 
                   G_CALLBACK (textview_key_press), editor);

  GObject* button_refresh = gtk_builder_get_object(builder, "button_refresh");
  g_signal_connect(G_OBJECT (button_refresh), "clicked", 
                   G_CALLBACK (mem_view_refresh), editor);

  GObject* button_up = gtk_builder_get_object(builder, "button_up");
  g_signal_connect(G_OBJECT (button_up), "clicked", 
                   G_CALLBACK (mem_view_up), editor);

  GObject* button_down = gtk_builder_get_object(builder, "button_down");
  g_signal_connect(G_OBJECT (button_down), "clicked", 
                   G_CALLBACK (mem_view_down), editor);

  return;
}
