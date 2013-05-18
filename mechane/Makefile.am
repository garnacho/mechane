SUBDIRS = backends

BUILT_SOURCES = 		\
	mech-marshal.c		\
	mech-marshal.h		\
	mech-enum-types.c	\
	mech-enum-types.h

mech_sources = 			\
	$(BUILT_SOURCES)	\
	mech-area.c		\
	mech-activatable.c	\
	mech-animation.c	\
	mech-acceleration.c	\
	mech-backend.c		\
	mech-button.c		\
	mech-clock.c		\
	mech-cursor.c		\
	mech-events.c		\
	mech-fixed-box.c	\
	mech-floating-box.c	\
	mech-linear-box.c	\
	mech-monitor.c		\
	mech-monitor-layout.c	\
	mech-orientable.c	\
	mech-parser.c		\
	mech-pattern.c		\
	mech-renderer.c		\
	mech-seat.c		\
	mech-stage.c		\
	mech-style.c		\
	mech-surface.c		\
	mech-theme.c		\
	mech-toggle.c		\
	mech-toggle-button.c	\
	mech-types.c		\
	mech-window-frame.c	\
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
