bin_PROGRAMS = gamecheater
gamecheater_SOURCES = gamecheater.h \
                      gamecheater.c \
                      search.c \
                      editor.c 

gamecheater_LDADD = $(DEPS_LIBS) ../libgcheater/libgcheater.la
AM_CPPFLAGS = $(DEPS_CFLAGS) \
              -I../libgcheater \
              -DPIXMAPS_DIR=\""$(datadir)/$(PACKAGE)/pixmaps"\" \
              -DGLADE_DIR=\""$(datadir)/$(PACKAGE)/glade"\" \
              -DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\"
