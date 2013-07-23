AM_CPPFLAGS = $(MECH_DEPS_CFLAGS)

MECHANE_DEMO_LDADDS = \
	$(MECH_DEPS_LIBS) \
	$(top_builddir)/mechane/libmechane-@MECH_API_VERSION@.la
MECHANE_DEMO_DEPS =

BUILT_SOURCES = \
	mechane-demo-resources.c \
	mechane-demo-resources.h

bin_PROGRAMS = 	\
	mechane-demo

mechane_demo_DEPENDENCIES = $(MECHANE_DEMO_DEPS)
mechane_demo_LDADD = $(MECHANE_DEMO_LDADDS)
mechane_demo_SOURCES = 		\
	$(BUILT_SOURCES) 	\
	mechane-demo.c

mechane-demo-resources.c: mechane-demo.gresource.xml mechane-demo.style
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/mechane-demo.gresource.xml \
                     --target=$@ --sourcedir=$(srcdir) --c-name mech_demo --generate-header
mechane-demo-resources.h: mechane-demo.gresource.xml mechane-demo.style
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/mechane-demo.gresource.xml \
                     --target=$@ --sourcedir=$(srcdir) --c-name mech_demo --generate-source

CLEANFILES = $(BUILT_SOURCES)
