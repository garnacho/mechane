mech_wayland_sources =			\
	mech-backend-wayland.c		\
	mech-event-source-wayland.c

AM_CPPFLAGS = $(MECH_DEPS_CFLAGS) $(MECH_WAYLAND_DEPS_CFLAGS)

lib_LTLIBRARIES = libmechane-wayland-@MECH_API_VERSION@.la
libmechane_waylandincludedir = $(includedir)/mechane-$(MECH_API_VERSION)/mechane/
libmechane_wayland_@MECH_API_VERSION@_la_SOURCES = $(mech_wayland_sources)
libmechane_wayland_@MECH_API_VERSION@_la_LIBADD = $(MECH_DEPS_LIBS) $(MECH_WAYLAND_DEPS_LIBS)
