#!/bin/sh
# Run this to generate all the initial makefiles, etc.

# Generate required files
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
(
  cd "$srcdir" &&
  touch ChangeLog && # Automake requires that ChangeLog exist
  mkdir -p m4 && # Aclocal requires that the m4 dir exists
  gtkdocize &&
  intltoolize &&
  autopoint --force &&
  AUTOPOINT='intltoolize --automake --copy' autoreconf --verbose --force --install
) || exit
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
