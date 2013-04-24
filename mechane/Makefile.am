BUILT_SOURCES = 		\
	mech-marshal.c		\
	mech-marshal.h

mech_sources = 			\
	$(BUILT_SOURCES)

AM_CPPFLAGS = $(MECH_DEPS_CFLAGS)

lib_LTLIBRARIES = libmechane-@MECH_API_VERSION@.la
libmechaneincludedir = $(includedir)/mechane-$(MECH_API_VERSION)/mechane/
libmechane_@MECH_API_VERSION@_la_SOURCES = $(mech_sources)
libmechane_@MECH_API_VERSION@_la_LIBADD = $(MECH_DEPS_LIBS)

mech-marshal.h: mech-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --prefix=_mech_marshal --header > $@

mech-marshal.c: mech-marshal.list
	$(AM_V_GEN) (echo "#include \"mech-marshal.h\""; \
	             $(GLIB_GENMARSHAL) $< --prefix=_mech_marshal --body) > $@

CLEANFILES = $(BUILT_SOURCES)
