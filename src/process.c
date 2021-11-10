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
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>

#include "search.h"
#include "process.h"


gboolean str2u64(const char* str, guint64* pu64) {
  if (str == NULL || pu64 == NULL) return FALSE;

  guint64 u64 = 0;
  int t = -1;
  int x = 10;
  int i = 0;
  const char* p = str;

  for (i = 0; ; i++) {
    if (p[i] == '-' || p[i] == '\0') return FALSE;
    if (p[i] == ' ' || p[i] == '+') continue;
    else break;
  }
  p += i;

  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    x = 0x10;
    p += 2;
  }

  if (p[0] == '\0') return FALSE;
  for (i = 0; ; i++) {
    t = -1;
    if (p[i] == '\0') break;
    if (p[i] >= '0' && p[i] <= '9') t = p[i] - 0x30;
    else if (p[i] >= 'a' && p[i] <= 'f' && x == 0x10) t = p[i] - 0x57;
    else if (p[i] >= 'A' && p[i] <= 'F' && x == 0x10) t = p[i] - 0x37;
    if (t < 0) return FALSE;
    u64 = u64 * x + t;
  }

  *pu64 = u64;

  return TRUE;
}

void get_mem_preview(unsigned long pid, void* addr, 
                     void* preview, unsigned long len)
{
  if (pid == 0 || addr == 0 || preview == NULL || len == 0) return; 

  errno = 0;
  ptrace(PTRACE_ATTACH, pid, 0, 0);
  if (errno != 0) goto out;

  waitpid(pid, NULL, 0); // wait child process stop
  if (errno != 0) goto out;

  memset(preview, 0, len);
  long x = 0;

  int i = 0;
  int j = len / sizeof(long);
  for (i = 0; i < j; i++) {
    x = ptrace(PTRACE_PEEKDATA, pid,
        addr + i*sizeof(long), x);
    if (errno != 0) goto out;
    memcpy(preview + i*sizeof(long), &x, sizeof(long));
  }

  j = len % sizeof(long);
  if (j > 0) {
    x = ptrace(PTRACE_PEEKDATA, pid, addr + i*sizeof(long), x);
    if (errno != 0) goto out;
    memcpy(preview + i*sizeof(long), &x, j);
  }

out:
// perrno("error:");
  ptrace(PTRACE_DETACH, pid, 0, SIGCONT);
  errno = 0;

  return;
}

gboolean ptrace_test(unsigned long pid)
{
  if (pid == 0) return FALSE;

  errno = 0;
  ptrace(PTRACE_ATTACH, pid, 0, 0);
//  if (errno != 0) goto out;

//printf("ptrace succesully!\n");

  waitpid(pid, NULL, 0); // wait child process stop
//  if (errno != 0) goto out;

//    if (errno != 0) {
//printf("not stopped!\n");
//    }

//out:
  ptrace(PTRACE_DETACH, pid, 0, SIGCONT);
  if (errno != 0) {
    errno = 0;
    return FALSE;
  } else {
    return TRUE;
  }

  return FALSE;
}

/* long search thread function */
void* search_value(cheater_t* cheater)
{
  if (cheater == NULL) return NULL;

  if (cheater->xml == NULL) return NULL;

  GladeXML* xml = cheater->xml;
  GtkWidget* window = glade_xml_get_widget(xml, "window");

  if (!GTK_IS_WINDOW (window)) return NULL;
  gdk_threads_enter();

  gtk_widget_set_sensitive(window, FALSE);
  GtkWidget* progressbar = glade_xml_get_widget(xml, "progressbar");

  gdk_threads_leave();

  FILE* fmaps = NULL;
  char buf[1024];

  errno = 0;
  ptrace(PTRACE_ATTACH, cheater->pid, 0, 0);
  if (errno != 0) goto out;

  waitpid(cheater->pid, NULL, 0);
  if (errno != 0) goto out;

  long x = 0;
  void* addr = NULL;

  if (cheater->result == NULL) {

    /* No results. get mem maps */
    snprintf(buf, sizeof(buf), "/proc/%lu/maps", cheater->pid);
    fmaps = fopen(buf, "rb");
    if (fmaps == NULL) goto out;

    while (fgets(buf, sizeof(buf), fmaps) != NULL) {
      unsigned long begin, end, offset, inode;
      int dev_major, dev_minor;
      char perms[5];
      char fname[1024];
      sscanf(buf, "%lx-%lx %s %lx %d:%d %lu %s", 
        &begin, &end, perms, &offset, &dev_major, &dev_minor, &inode, fname);
      // skip read only mem
      if (perms[1] != 'w') continue;
      // skip device mem
      if (strncmp(fname, "/dev/", 5) == 0) continue;
      // skip system library mem
      if (strncmp(fname, "/lib/", 5) == 0) continue;
      if (strncmp(fname, "/usr/X11R6/", 11) == 0) continue;
      // skip system library mem. FIXME! Maybe game library?
      if (strncmp(fname, "/usr/lib/", 9) == 0) continue;
#ifdef DEBUG
printf("%08X-%08X %s %08X %02d:%02d %lu %s\n", 
    begin, end, perms, offset, dev_major, dev_minor, inode, fname);
#endif

      unsigned long j = 0;
      unsigned long k = end - begin;
      unsigned long len = 0;
      unsigned char mem[MEMORY_BLOCK_SIZE];

      if (!GTK_IS_WINDOW (window)) goto out;
      gdk_threads_enter();

      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (progressbar), 0.0);

      gdk_threads_leave();

      addr = (void*) begin;
      while ((unsigned long)addr < end) {
        if ((unsigned long) addr + MEMORY_BLOCK_SIZE <= end)
          len = MEMORY_BLOCK_SIZE;
        else len = end - (unsigned long) addr;

        unsigned long i = 0;
        for (i = 0; i < len / sizeof(long); i++) {
          x = ptrace(PTRACE_PEEKDATA, cheater->pid, addr + i*sizeof(long), x);
          if (errno != 0) { // skip this range
#ifdef DEBUG
perror("peek error!\n");
printf("peek 0x%08X error, errno = %d\n", (unsigned long) addr, errno);
#endif
            break;
          }
          memcpy(mem + i*sizeof(long), &x, sizeof(long));
        }
        if (errno != 0) {
          errno = 0;
          break;
        }

        if (len % sizeof(long) != 0) {
          x = ptrace(PTRACE_PEEKDATA, cheater->pid, addr + i*sizeof(long), x);
          if (errno != 0) { // skip this range
#ifdef DEBUG
perror("peek error!\n");
printf("peek 0x%08X error, errno = %d\n", (unsigned long) addr, errno);
#endif
            break;
          }
          memcpy(mem + i*sizeof(long), &x, len % sizeof(long));
        }
        if (errno != 0) {
          errno = 0;
          break;
        }

        unsigned long off = 0;

        switch (cheater->type) {
          case TYPE_U8 :
          {
            guint8* pu8 = (guint8*) mem;
            off = len / sizeof(guint8);
            i = 0;
            while (i < off) {
              if (pu8[i] == (guint8) cheater->value) {
                cheater->result = g_slist_append(cheater->result, 
                    addr + sizeof(guint8) * i);
              }
              i++;
            }
            break;
          }
          case TYPE_U16 :
          {
            guint16* pu16 = (guint16*) mem;
            off = len / sizeof(guint16);
            i = 0;
            while (i < off) {
              if (pu16[i] == (guint16) cheater->value) {
                cheater->result = g_slist_append(cheater->result, 
                    addr + sizeof(guint16) * i);
              }
              i++;
            }
            break;
          }
          case TYPE_U32 :
          {
            guint32* pu32 = (guint32*) mem;
            off = len / sizeof(guint32);
            i = 0;
            while (i < off) {
              if (pu32[i] == (guint32) cheater->value) {
                cheater->result = g_slist_append(cheater->result, 
                    addr + sizeof(guint32) * i);
              }
              i++;
            }
            break;
          }
          case TYPE_U64 :
          {
            guint64* pu64 = (guint64*) mem;
            off = len / sizeof(guint64);
            i = 0;
            while (i < off) {
              if (pu64[i] == (guint64) cheater->value) {
                cheater->result = g_slist_append(cheater->result, 
                    addr + sizeof(guint64) * i);
              }
              i++;
            }
            break;
          }
          default :
            break;
        } // end of switch()

        if ((j % MEMORY_BLOCK_SIZE) == 0) {
          if (!GTK_IS_WINDOW (window)) goto out;
          gdk_threads_enter();

          gdouble percent = (gdouble) j / (gdouble) k;
          gtk_progress_bar_set_fraction(
             GTK_PROGRESS_BAR (progressbar), percent);
          snprintf(buf, sizeof(buf), "0x%0*lX-0x%0*lX: %05.2f%%",
             sizeof(long)*2, begin, sizeof(long)*2, end, percent*100);
          gtk_progress_bar_set_text(GTK_PROGRESS_BAR (progressbar), buf);

          gdk_threads_leave();
        }

        j += len;
        addr += len;
      } // end of while (addr < end)


    } // end of while (fgets())
    fclose(fmaps);
    fmaps = NULL;

  } else {
    /* value changed. search again */
    GSList* list = cheater->result;

    unsigned long j = 0;
    unsigned long k = g_slist_length(cheater->result);

    if (!GTK_IS_WINDOW (window)) goto out;
    gdk_threads_enter();

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (progressbar), 0.0);

    gdk_threads_leave();

    while (1) {
      addr = list->data;
      x = ptrace(PTRACE_PEEKDATA, cheater->pid, addr, x);
      if (errno != 0) {
#ifdef DEBUG
perror("2:peek error: ");
printf("2:peek 0x%08X error, errno = %d\n", (unsigned long) addr, errno);
#endif
        errno = 0;
//        break;
      }

      long* pt = &x;

      switch (cheater->type) {
        case TYPE_U8 :
        {
          guint8* pu8 = (guint8*) pt;
          if (pu8[0] != (guint8) cheater->value) {
            list->data = NULL;
          }
          break;
        }
        case TYPE_U16 :
        {
          guint16* pu16 = (guint16*) pt;
          if (pu16[0] != (guint16) cheater->value) {
            list->data = NULL;
          }
          break;
        }
        case TYPE_U32 :
        {
          guint32* pu32 = (guint32*) pt;
          if (pu32[0] != (guint32) cheater->value) {
            list->data = NULL;
          }
          break;
        }
        case TYPE_U64 :
        {
          /* For 32-bit platform search 64-bit length value */
          if (sizeof(guint64) / sizeof(long) == 2) {
            long y = ptrace(PTRACE_PEEKDATA, cheater->pid, 
                addr + sizeof(long), y);
            if (errno != 0) {
              errno = 0;
              break;
            }
            guint64 u64 = 0;
            memcpy(&u64, &x, sizeof(long));
            memcpy((void*) &u64 + sizeof(long), &y, sizeof(long));
            if (u64 != (guint64) cheater->value) {
              list->data = NULL;
            }
          } else {
            guint64* pu64 = (guint64*) pt;
            if (pu64[0] != (guint64) cheater->value) {
              list->data = NULL;
            }
          }
          break;
        }
        default :
          break;
      } // end of switch()

      list = list->next;
      if (list == NULL) break;

      if ((j % 0x100) == 0) {
        if (!GTK_IS_WINDOW (window)) goto out;
        gdk_threads_enter();

        gdouble percent = (gdouble) ((gdouble) j / (gdouble) k);
        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR (progressbar), percent);
        snprintf(buf, sizeof(buf), "%lu/%lu %05.2f%%",
            j, k, percent*100);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR (progressbar), buf);

        gdk_threads_leave();
      }
      j++;

    } // end of while (1)

    cheater->result = g_slist_remove_all(cheater->result, NULL);
  }

out:
  ptrace(PTRACE_DETACH, cheater->pid, 0, SIGCONT);
  errno = 0;

  if (fmaps != NULL) fclose(fmaps);

  if (!GTK_IS_WINDOW (window)) return NULL;
  gdk_threads_enter();

  gtk_widget_set_sensitive(window, TRUE);

  GtkWidget* treeview = glade_xml_get_widget(xml, "treeview");
  refresh_treeview(GTK_TREE_VIEW (treeview), cheater);

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (progressbar), 0.0);
  snprintf(buf, sizeof(buf), "%u matched", g_slist_length(cheater->result));
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR (progressbar), buf);

  gdk_threads_leave();

  return NULL;
}

void update_value(unsigned long pid, void* addr, 
                  void* value, unsigned long len)
{
  if (pid == 0 || addr == NULL || value == NULL || len == 0) return;

  errno = 0;
  ptrace(PTRACE_ATTACH, pid, 0, 0);
  if (errno != 0) goto out;

  // wait child process stop
  waitpid(pid, NULL, 0);
  if (errno != 0) goto out;

  unsigned long i = 0;
  long x = 0;
  for (i = 0; i < len / sizeof(long); i++) {
    memcpy(&x, value + i*sizeof(long), sizeof(long));
    ptrace(PTRACE_POKEDATA, pid, addr + i*sizeof(long), x);
    if (errno != 0) goto out;
  }

  if (len % sizeof(long) != 0) {
    x = ptrace(PTRACE_PEEKDATA, pid, addr, x);
    if (errno != 0) goto out;
    memcpy(&x, value + i*sizeof(long), len % sizeof(long));
    ptrace(PTRACE_POKEDATA, pid, addr + i*sizeof(long), x);
    if (errno != 0) goto out;
  }

out:
  ptrace(PTRACE_DETACH, pid, 0, SIGCONT);
  errno = 0;

  return;
}

