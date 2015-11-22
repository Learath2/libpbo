#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libpbo], [0.1.0], [])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/testlibpbo.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
LT_PREREQ([2.4])
AM_PROG_AR
LT_INIT

AM_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_CHECK_HEADERS([stdlib.h])

AC_CONFIG_FILES([Makefile
		 include/Makefile
		 include/libpbo/Makefile
		 libpbo/Makefile
                 src/Makefile])
AC_OUTPUT
echo \
"-------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}

 Prefix: '${prefix}'.
 Compiler: '${CC} ${CFLAGS}'
 Libraries: '${LIBS}'

--------------------------------------------------"
