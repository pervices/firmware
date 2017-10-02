#!/bin/sh

if [ "$1" = "clean" ]; then
  make clean &>/dev/null
  make distclean &>/dev/null
  make maintainer-clean &>/dev/null
  rm -Rf \
    $(find . \
      -name '*~' \
      -o -name 'config.h' \
      -o -name 'config.h.in' \
      -o -name 'Makefile.in' \
      -o -name 'Makefile' \
      -o -name 'aclocal.m4' \
      -o -name 'ar-lib' \
      -o -name 'autom4te.cache' \
      -o -name 'compile' \
      -o -name 'py-compile' \
      -o -name 'config.guess' \
      -o -name 'config.status' \
      -o -name 'config.sub' \
      -o -name 'configure' \
      -o -name 'depcomp' \
      -o -name 'install-sh' \
      -o -name 'ltmain.sh' \
      -o -name 'libtool' \
      -o -name 'libtool.m4' \
      -o -name 'ltoptions.m4' \
      -o -name 'ltsugar.m4' \
      -o -name 'ltversion.m4' \
      -o -name 'lt~obsolete.m4' \
      -o -name 'm4' \
      -o -name 'missing' \
      -o -name 'stamp-h1' \
      -o -name 'test-driver' \
      -o -name '.deps' \
      -o -name '.libs' \
      -o -name '.dirstamp' \
      -o -name '*.o' \
      -o -name '*.lo' \
      -o -name '*.la' \
      -o -name '*.log' \
      -o -name '*.scan' \
      -o -name 'INSTALL' \
      -o -name '*.doxyfile' \
  )
  exit 0
fi

autoreconf -vif
