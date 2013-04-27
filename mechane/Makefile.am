SUBDIRS = backends

BUILT_SOURCES = 		\
	mech-marshal.c		\
	mech-marshal.h		\
	mech-enum-types.c	\
	mech-enum-types.h

mech_sources = 			\
	$(BUILT_SOURCES)	\
	mech-area.c		\
	mech-backend.c		\
	mech-events.c		\
	mech-stage.c		\
	mech-surface.c		\
	mech-types.c		\
	mech-window.c

AM_CPPFLAGS = $(MECH_DEPS_CFLAGS)

lib_LTLIBRARIES = libmechane-@MECH_API_VERSION@.la
libmechaneincludedir = $(includedir)/mechane-$(MECH_API_VERSION)/mechane/
libmechane_@MECH_API_VERSION@_la_SOURCES = $(mech_sources)
libmechane_@MECH_API_VERSION@_la_LIBADD = $(MECH_DEPS_LIBS) $(top_builddir)/mechane/backends/wayland/libmechane-wayland-@MECH_API_VERSION@.la

mech-enum-types.h: mech-enums.h mech-enum-types.h.template
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template mech-enum-types.h.template mech-enums.h) > $@

mech-enum-types.c: mech-enums.h mech-enum-types.c.template
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template mech-enum-types.c.template mech-enums.h) > $@

mech-marshal.h: mech-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --prefix=_mech_marshal --header > $@

mech-marshal.c: mech-marshal.list
	$(AM_V_GEN) (echo "#include \"mech-marshal.h\""; \
	             $(GLIB_GENMARSHAL) $< --prefix=_mech_marshal --body) > $@

CLEANFILES = $(BUILT_SOURCES)
