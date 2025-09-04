#!/bin/sh

if [ "$1" = "clean" ]; then
  make clean &>/dev/null
  make distclean &>/dev/null
  make maintainer-clean &>/dev/null
  rc=0
  clean_list=('*~' \
      'config.h' \
      'config.h.in' \
      'Makefile.in' \
      'Makefile' \
      'aclocal.m4' \
      'ar-lib' \
      'autom4te.cache' \
      'compile' \
      'py-compile' \
      'config.guess' \
      'config.status' \
      'config.sub' \
      'configure' \
      'depcomp' \
      'install-sh' \
      'ltmain.sh' \
      'libtool' \
      'libtool.m4' \
      'ltoptions.m4' \
      'ltsugar.m4' \
      'ltversion.m4' \
      'lt~obsolete.m4' \
      'm4' \
      'missing' \
      'stamp-h1' \
      'test-driver' \
      '.deps' \
      '.libs' \
      '.dirstamp' \
      '*.o' \
      '*.lo' \
      '*.la' \
      '*.log' \
      '*.scan' \
      'INSTALL' \
      '*.doxyfile' \
  )
  for i in ${clean_list[@]};
  do
    rm -rf $(find -name $i) || rc=$?
  done
  exit $rc
fi

autoreconf -vif
