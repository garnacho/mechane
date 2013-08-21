SUBDIRS = backends

BUILT_SOURCES = 		\
	mech-marshal.c		\
	mech-marshal.h		\
	mech-enum-types.c	\
	mech-enum-types.h	\
	mech-resources.c	\
	mech-resources.h

mech_sources = 			\
	$(BUILT_SOURCES)	\
	mech-area.c		\
	mech-activatable.c	\
	mech-adjustable.c	\
	mech-animation.c	\
	mech-acceleration.c	\
	mech-backend.c		\
	mech-button.c		\
	mech-clock.c		\
	mech-check-mark.c	\
	mech-container.c	\
	mech-controller.c	\
	mech-cursor.c		\
	mech-events.c		\
	mech-fixed-box.c	\
	mech-floating-box.c	\
	mech-gesture.c		\
	mech-gesture-drag.c	\
	mech-gesture-swipe.c	\
	mech-image.c		\
	mech-linear-box.c	\
	mech-monitor.c		\
	mech-monitor-layout.c	\
	mech-orientable.c	\
	mech-parser.c		\
	mech-pattern.c		\
	mech-renderer.c		\
	mech-seat.c		\
	mech-scrollable.c	\
	mech-scroll-area.c	\
	mech-scroll-box.c	\
	mech-slider.c		\
	mech-stage.c		\
	mech-style.c		\
	mech-surface.c		\
	mech-text.c		\
	mech-text-attributes.c	\
	mech-text-buffer.c	\
	mech-text-input.c	\
	mech-text-range.c	\
	mech-text-view.c	\
	mech-theme.c		\
	mech-toggle.c		\
	mech-toggle-button.c	\
	mech-types.c		\
	mech-view.c		\
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

mech-resources.c: mechane.gresource.xml $(srcdir)/theme/style
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/mechane.gresource.xml \
	             --target=$@ --sourcedir=$(srcdir) --c-name _mech --generate-header
mech-resources.h: mechane.gresource.xml $(srcdir)/theme/style
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/mechane.gresource.xml \
	             --target=$@ --sourcedir=$(srcdir) --c-name _mech --generate-source


CLEANFILES = $(BUILT_SOURCES)
