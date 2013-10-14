mech_wayland_sources =			\
	subsurface-protocol.c		\
	mech-backend-wayland.c		\
	mech-clock-wayland.c		\
	mech-cursor-wayland.c		\
	mech-egl-config.c		\
	mech-event-source-wayland.c	\
	mech-monitor-wayland.c		\
	mech-seat-wayland.c		\
	mech-surface-wayland.c		\
	mech-surface-wayland-egl.c	\
	mech-surface-wayland-shm.c	\
	mech-surface-wayland-texture.c	\
	mech-window-wayland.c

AM_CPPFLAGS = $(CFLAGS) $(MECH_DEPS_CFLAGS) $(MECH_WAYLAND_DEPS_CFLAGS) $(MECH_EGL_DEPS_CFLAGS)

lib_LTLIBRARIES = libmechane-wayland-@MECH_API_VERSION@.la
libmechane_waylandincludedir = $(includedir)/mechane-$(MECH_API_VERSION)/mechane/
libmechane_wayland_@MECH_API_VERSION@_la_SOURCES = $(mech_wayland_sources)
libmechane_wayland_@MECH_API_VERSION@_la_LIBADD = $(MECH_DEPS_LIBS) $(MECH_WAYLAND_DEPS_LIBS) $(MECH_EGL_DEPS_LIBS)
