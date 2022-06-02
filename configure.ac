AC_PREREQ([2.69])
AC_INIT([binbloom], [2.0], [dcauquil@quarkslab.com])
AC_CONFIG_SRCDIR([src/common.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([m], [log2])
AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h sys/ioctl.h unistd.h byteswap.h sys/endian.h machine/endian.h sys/byteorder.h sys/isa_defs.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strtol])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
