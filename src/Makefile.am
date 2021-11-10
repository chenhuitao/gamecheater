bin_PROGRAMS = gamecheater
gamecheater_SOURCES = gamecheater.h \
                      gamecheater.c \
                      process.h \
                      process.c \
                      search.h \
                      search.c \
                      editor.h \
                      editor.c 

gamecheater_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CFLAGS) \
              -DPIXMAPS_DIR=\""$(datadir)/$(PACKAGE)/pixmaps"\" \
              -DGLADE_DIR=\""$(datadir)/$(PACKAGE)/glade"\" \
              -DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\"
