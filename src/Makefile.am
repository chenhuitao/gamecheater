bin_PROGRAMS = gamecheater
gamecheater_SOURCES = gamecheater.h \
                      gamecheater.c \
                      search.c \
                      editor.c 

gamecheater_LDADD = $(DEPS_LIBS) ../libgcheater/libgcheater.la
AM_CPPFLAGS = $(DEPS_CFLAGS) \
              -I../libgcheater \
              -DPIXMAPS_DIR=\""$(datadir)/$(PACKAGE)/pixmaps"\" \
              -DUI_DIR=\""$(datadir)/$(PACKAGE)/ui"\" \
              -DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" \
              -DGTK_DISABLE_SINGLE_INCLUDES \
              -DGDK_DISABLE_DEPRECATED \
              -DGTK_DISABLE_DEPRECATED \
              -DGSEAL_ENABLE
