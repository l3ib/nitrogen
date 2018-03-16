AC_PREREQ([2.69])
AC_INIT([nitrogen],[1.6.1],[daf@minuslab.net])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR([.])
AC_ISC_POSIX
AC_HEADER_STDC++
AM_INIT_AUTOMAKE([-Wno-obsolete])

AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl
dnl Our depend list
dnl

GLIB2_VERSION=2.6.0
GTK2_VERSION=2.10.0
GTKMM2_VERSION=2.10.0
GTHREAD2_VERSION=2.6.0

AC_SUBST([GLIB2_VERSION])
AC_SUBST([GTK2_VERSION])

PKG_CHECK_MODULES([GLIB2], [glib-2.0 >= $GLIB2_VERSION])
PKG_CHECK_MODULES([GTK2], [gtk+-2.0 >= $GTK2_VERSION])
PKG_CHECK_MODULES([GTKMM2], [gtkmm-2.4 >= $GTKMM2_VERSION])
PKG_CHECK_MODULES([GTHREAD2], [gthread-2.0 >= $GTHREAD2_VERSION])

NITROGEN_LIBS="$GLIB2_LIBS $GTK2_LIBS $GTKMM2_LIBS $GTHREAD2_LIBS"
NITROGEN_CFLAGS="$GLIB2_CFLAGS $GTK2_CFLAGS $GTKMM2_CFLAGS $GTHREAD2_CFLAGS"

dnl apparantly we have to check for X11 now
AC_CHECK_LIB(X11, XOpenDisplay, [NITROGEN_LIBS="$NITROGEN_LIBS -lX11"])

ALL_LINGUAS=""
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.7])

AC_LANG([C++])
AC_LANG_COMPILER_REQUIRE
AC_HEADER_STDC([])

# Check for header files
AC_CHECK_HEADERS([libintl.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_CHECK_FUNCS([select])

# Get autotools to use inotify
AC_ARG_ENABLE([inotify], [  --disable-inotify       disable support for inotify watching of dirs (default: enabled)], [enable_inotify=$enableval],[enable_inotify=yes])

AC_ARG_ENABLE([debug], [  --enable-debug       enable debugging flags ])

if test "$enable_debug" = "yes"; then
 CXXFLAGS+=" -g -g3 -ggdb -DDEBUG"
fi

if test x$enable_inotify = xyes; then
	AC_DEFINE([USE_INOTIFY], 1, [Define to enable inotify dir watching])
fi

# Check for Xinerama
AC_ARG_ENABLE([xinerama], [  --disable-xinerama      disable support for Xinerama (default: enabled)], [enable_xinerama=$enableval],[enable_xinerama=yes])

if test x$enable_xinerama = xyes; then
	AC_CHECK_LIB([Xinerama], [XineramaQueryScreens], 
				 	[AC_DEFINE(USE_XINERAMA, 1, [Define to enable Xinerama])
					 NITROGEN_LIBS="$NITROGEN_LIBS -lXinerama"],
					[enable_xinerama=no])
fi

AC_SUBST([NITROGEN_LIBS])
AC_SUBST([NITROGEN_CFLAGS])

AC_CONFIG_FILES([Makefile 
                 po/Makefile.in
                 data/Makefile
                 data/nitrogen.desktop
                 data/icons/Makefile
                 src/Makefile])
AC_OUTPUT

# syscrash says don't test for inotify support in kernel for some reason
echo ""
echo "##########################################"
echo ""
echo "Options:"
echo ""

if test x$enable_inotify = xyes; then
	echo "inotify support ............. enabled"
else
	echo "inotify support ............. disabled"
fi

if test x$enable_xinerama = xyes; then
	echo "xinerama support ............ enabled"
else
	echo "xinerama support ............ disabled"
fi

echo ""
echo "##########################################"
echo ""
echo "You are now ready to compile nitrogen"
echo "Type \"make\" to compile nitrogen"
