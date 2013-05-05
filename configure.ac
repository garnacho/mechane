m4_define([mech_major_version], [0])
m4_define([mech_minor_version], [1])
m4_define([mech_micro_version], [0])
m4_define([mech_interface_age], [0])
m4_define([mech_binary_age],
          [m4_eval(100 * mech_minor_version + mech_micro_version)])
m4_define([mech_version],
          [mech_major_version.mech_minor_version.mech_micro_version])
# This is the X.Y used in -lmechane-X.Y
m4_define([mech_api_version], [0.1])

AC_PREREQ([2.62])
AC_INIT([mechane], [mech_version],
        [],
        [mechane])

IT_PROG_INTLTOOL

# libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# automake
AM_INIT_AUTOMAKE([1.11 no-define tar-ustar -Wno-portability])
AM_SILENT_RULES([yes])

# versions we depend on
GLIB_VERSION=2.37.4
CAIRO_VERSION=1.10
PANGO_VERSION=1.28
LIBRSVG_VERSION=2.36
WAYLAND_VERSION=1.0
XKB_COMMON_VERSION=0.2

# glib
AM_PATH_GLIB_2_0($GLIB_VERSION, , AC_MSG_ERROR([
*** GLIB $GLIB_VERSION or newer is required.]), gobject)

# gtk-doc
GTK_DOC_CHECK([1.11],[--flavour no-tmpl])

# i18n
AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The gettext translation domain])

AC_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([mechane/mech-marshal.list])
AC_CONFIG_MACRO_DIR([m4])

# export variables
MECH_MAJOR_VERSION=mech_major_version
MECH_MINOR_VERSION=mech_minor_version
MECH_MICRO_VERSION=mech_micro_version
MECH_INTERFACE_AGE=mech_interface_age
MECH_BINARY_AGE=mech_binary_age
MECH_VERSION=mech_version
MECH_API_VERSION=mech_api_version
MECH_BINARY_VERSION=mech_binary_version

AC_SUBST(MECH_API_VERSION)

#general deps
PKG_CHECK_MODULES(MECH_DEPS, [
        gobject-2.0 >= $GLIB_VERSION
        gio-2.0 >= $GLIB_VERSION
        cairo >= $CAIRO_VERSION
        cairo-gobject >= $CAIRO_VERSION
        pango >= $PANGO_VERSION
        pangocairo >= $PANGO_VERSION
        librsvg-2.0 >= $LIBRSVG_VERSION
        ])

# wayland deps
PKG_CHECK_MODULES(MECH_WAYLAND_DEPS, [
        wayland-client >= $WAYLAND_VERSION
        wayland-cursor >= $WAYLAND_VERSION
        xkbcommon >= $XKB_COMMON_VERSION
        ])

AC_CONFIG_FILES([
	Makefile
        mechane/backends/Makefile
        mechane/backends/wayland/Makefile
	mechane/Makefile
	po/Makefile.in
])

AC_OUTPUT

echo "Mechane $MECH_VERSION"
echo ""
