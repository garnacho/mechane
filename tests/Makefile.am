AM_CPPFLAGS = \
	$(MECH_DEPS_CFLAGS)

TEST_LDADDS = \
	$(MECH_DEPS_LIBS) \
	$(top_builddir)/mechane/libmechane-@MECH_API_VERSION@.la
TEST_DEPS =

noinst_PROGRAMS = 		\
	test-text-entry

test_text_entry_DEPENDENCIES = $(TEST_DEPS)
test_text_entry_LDADD = $(TEST_LDADDS)
