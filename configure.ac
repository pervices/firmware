#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([crimson-firmware], [9999], [solutions@pervices.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([mem.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_AR

AC_CHECK_PROG([GIT],git,git,AC_MSG_ERROR([git executable not in PATH]))
AC_CHECK_PROG([date],date,date,AC_MSG_ERROR([date executable not in PATH]))

# Checks for libraries.
AC_CHECK_LIB([m], [pow])
AC_CHECK_LIB([rt], [clock_gettime])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([clock_gettime floor getcwd gettimeofday inet_ntoa memmove memset mkdir munmap pow realpath select socket strerror strstr])

CPPFLAGS="${CPPFLAGS}"
CPPFLAGS+=" -I\${top_srcdir}/common"
CPPFLAGS+=" -I\${top_srcdir}/hal "
CPPFLAGS+=" -I\${top_srcdir}/hal/drivers/mmap"
CPPFLAGS+=" -I\${top_srcdir}/hal/drivers/uart"
CPPFLAGS+=" -I\${top_srcdir}/hal/drivers/udp"
CPPFLAGS+=" -I\${top_srcdir}/parser"

CFLAGS="${CFLAGS}"
CFLAGS+=" -DVERSIONGITBRANCH=\\\"`git rev-parse --abbrev-ref HEAD`\\\""
CFLAGS+=" -DVERSIONGITREVISION=\\\"`git describe --abbrev=8 --dirty --always --long`\\\""
CFLAGS+=" -DVERSIONDATE=\\\"`TZ=UTC date '+%F-%T'`\\\""

CFLAGS+=" -D${PRODUCT}"
CFLAGS+=" -D${HW_REV}"
CFLAGS+=" -D${NRX}"
CFLAGS+=" -D${NTX}"
CFLAGS+=" -D${MAX_RATE}"
CFLAGS+=" -DRX_40GHZ_FE=${RX_40GHZ_FE}"
CFLAGS+=" -DUSE_3G_AS_1G=${USE_3G_AS_1G}"
CFLAGS+=" -DS_USE_3G_AS_1G=\\\"${USE_3G_AS_1G}\\\""
CFLAGS+=" -DUSER_LO=${USER_LO}"

LDFLAGS="${LDFLAGS}"
LDFLAGS+=" -Wl,-no-undefined"

LT_INIT

AC_CONFIG_FILES([Makefile
                 common/Makefile
                 hal/Makefile
                 hal/drivers/Makefile
                 hal/drivers/mmap/Makefile
                 hal/drivers/uart/Makefile
                 hal/drivers/udp/Makefile
                 parser/Makefile])
AC_OUTPUT
