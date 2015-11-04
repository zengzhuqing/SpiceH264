AC_PREREQ([2.57])

# Follow the libtool manual for the so version:
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# - If the library source code has changed at all since the last update,
#   then increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# - If any interfaces have been added, removed, or changed since the last update,
#   increment current, and set revision to 0.
# - If any interfaces have been added since the last public release,
#   then increment age.
# - If any interfaces have been removed or changed since the last public release,
#   then set age to 0.
#
m4_define([SPICE_CURRENT], [11])
m4_define([SPICE_REVISION], [0])
m4_define([SPICE_AGE], [10])

AC_INIT(spice, [m4_esyscmd(build-aux/git-version-gen .tarball-version)],
        [spice-devel@lists.freedesktop.org], spice)

major=`echo $PACKAGE_VERSION | cut -d. -f1`
minor=`echo $PACKAGE_VERSION | cut -d. -f2`
micro=`echo $PACKAGE_VERSION | cut -d. -f3`
git=`echo $PACKAGE_VERSION | cut -d. -f4`
if test x"$git" != x ; then
    micro=$(($micro+1))
fi
SPICE_SERVER_VERSION=`printf "0x%02x%02x%02x" $major $minor $micro`
AC_SUBST(SPICE_SERVER_VERSION)

AC_CONFIG_MACRO_DIR([m4])
m4_include([spice-common/m4/spice-deps.m4])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR(.)

AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip subdir-objects])
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_CC_C99
if test x"$ac_cv_prog_cc_c99" = xno; then
    AC_MSG_ERROR([C99 compiler is required.])
fi
AC_PROG_INSTALL
AC_CANONICAL_HOST
AC_PROG_LIBTOOL
AM_PROG_CC_C_O
AC_C_BIGENDIAN
AC_PATH_PROGS(PYTHON, python2 python)
PKG_PROG_PKG_CONFIG

AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([execinfo.h])
AC_CHECK_HEADERS([linux/sockios.h])
AC_FUNC_ALLOCA

SPICE_LT_VERSION=m4_format("%d:%d:%d", SPICE_CURRENT, SPICE_REVISION, SPICE_AGE)
AC_SUBST(SPICE_LT_VERSION)

# For autoconf < 2.63
m4_ifndef([AS_VAR_APPEND],
          AC_DEFUN([AS_VAR_APPEND], $1=$$1$2))

# Check for the CPU we are using
case $host_cpu in
  x86_64)
        arch_warn=""
        ;;
  *)
        arch_warn="spice-server on non-x86_64 architectures hasn't been extensively tested"
esac

dnl =========================================================================
dnl Check optional features
SPICE_CHECK_OPENGL([GL])
AS_IF([test x"$enable_opengl" != "xno"], [
      AS_VAR_APPEND([SPICE_NONPKGCONFIG_LIBS], [" $GL_LIBS"])
])


SPICE_CHECK_SMARTCARD([SMARTCARD])
AM_CONDITIONAL(SUPPORT_SMARTCARD, test "x$have_smartcard" = "xyes")

AC_ARG_ENABLE([automated_tests],
              AS_HELP_STRING([--enable-automated-tests], [Enable automated tests using spicy-screenshot (part of spice--gtk)]),,
              [enable_automated_tests="no"])
AS_IF([test x"$enable_automated_tests" != "xno"], [enable_automated_tests="yes"])
AM_CONDITIONAL(SUPPORT_AUTOMATED_TESTS, test "x$enable_automated_tests" != "xno")

SPICE_CHECK_LZ4([LZ4])

dnl =========================================================================
dnl Check deps

AC_CONFIG_SUBDIRS([spice-common])
COMMON_CFLAGS='-I ${top_srcdir}/spice-common/ -I ${top_builddir}/spice-common/'
AC_SUBST(COMMON_CFLAGS)

AC_CHECK_LIBM
AC_SUBST(LIBM)

AC_CHECK_LIB(rt, clock_gettime, LIBRT="-lrt")
AC_SUBST(LIBRT)

AS_VAR_APPEND([SPICE_NONPKGCONFIG_LIBS], [" -pthread $LIBM $LIBRT"])

SPICE_REQUIRES=""

AS_IF([test x"$have_smartcard" = "xyes"], [
    AS_VAR_APPEND([SPICE_REQUIRES], [" libcacard >= 0.1.2"])
])

SPICE_PROTOCOL_MIN_VER=0.12.10
PKG_CHECK_MODULES([SPICE_PROTOCOL], [spice-protocol >= $SPICE_PROTOCOL_MIN_VER])
AC_SUBST([SPICE_PROTOCOL_MIN_VER])

PKG_CHECK_MODULES([GLIB2], [glib-2.0 >= 2.22])
AS_VAR_APPEND([SPICE_REQUIRES], [" glib-2.0 >= 2.22"])

PKG_CHECK_MODULES(PIXMAN, pixman-1 >= 0.17.7)
AC_SUBST(PIXMAN_CFLAGS)
AC_SUBST(PIXMAN_LIBS)
AS_VAR_APPEND([SPICE_REQUIRES], [" pixman-1 >= 0.17.7"])

AC_ARG_ENABLE([celt051],
              AS_HELP_STRING([--disable-celt051], [Disable celt051 audio codec (enabled by default)]),,
              [enable_celt051="yes"])

if test "x$enable_celt051" = "xyes"; then
    PKG_CHECK_MODULES(CELT051, celt051 >= 0.5.1.1, AS_VAR_APPEND([SPICE_REQUIRES], [" celt051 >= 0.5.1.1"]))
fi

if test ! -e client/generated_marshallers.cpp; then
AC_MSG_CHECKING([for pyparsing python module])
echo "import pyparsing" | ${PYTHON} - >/dev/null 2>&1
if test $? -ne 0 ; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([pyparsing python module is required to compile this package])
fi
AC_MSG_RESULT([found])
fi

PKG_CHECK_MODULES(SSL, openssl)
AC_SUBST(SSL_CFLAGS)
AC_SUBST(SSL_LIBS)
AS_VAR_APPEND([SPICE_REQUIRES], [" openssl"])

AC_CHECK_LIB(jpeg, jpeg_destroy_decompress,
    AC_MSG_CHECKING([for jpeglib.h])
    AC_TRY_CPP(
[#include <stdio.h>
#undef PACKAGE
#undef VERSION
#undef HAVE_STDLIB_H
#include <jpeglib.h>],
        JPEG_LIBS='-ljpeg'
        AC_MSG_RESULT($jpeg_ok),
	AC_MSG_ERROR([jpeglib.h not found])),
    AC_MSG_ERROR([libjpeg not found]))
AC_SUBST(JPEG_LIBS)

AC_CHECK_LIB(z, deflate, Z_LIBS='-lz', AC_MSG_ERROR([zlib not found]))
AC_SUBST(Z_LIBS)

dnl Cyrus SASL, check from gtk-vnc
AC_ARG_WITH([sasl],
  [AS_HELP_STRING([--with-sasl],
    [use cyrus SASL for authentication @<:@default=check@:>@])],
  [],
  [with_sasl=check])

SASL_CFLAGS=
SASL_LIBS=
enable_sasl=no
if test "x$with_sasl" != "xno"; then
  if test "x$with_sasl" != "xyes" && test "x$with_sasl" != "xcheck"; then
    SASL_CFLAGS="-I$with_sasl"
    SASL_LIBS="-L$with_sasl"
  fi
  fail=0
  old_cflags="$CFLAGS"
  old_libs="$LIBS"
  CFLAGS="$CFLAGS $SASL_CFLAGS"
  LIBS="$LIBS $SASL_LIBS"
  AC_CHECK_HEADER([sasl/sasl.h],[],[
    if test "x$with_sasl" != "xcheck" ; then
        with_sasl=no
    else
        fail=1
    fi])
  if test "x$with_sasl" != "xno" ; then
    AC_CHECK_LIB([sasl2], [sasl_client_init],[with_sasl2=yes],[with_sasl2=no])
  fi
  if test "x$with_sasl2" = "xno" && test "x$with_sasl" != "xno" ; then
    AC_CHECK_LIB([sasl], [sasl_client_init],[with_sasl=yes],[with_sasl=no])
  fi
  if test "x$with_sasl2" = "xyes"; then
    SASL_LIBS="$SASL_LIBS -lsasl2"
  elif test "x$with_sasl" = "xyes"; then
    SASL_LIBS="$SASL_LIBS -lsasl"
  else
    AC_MSG_ERROR([Missing required Cyrus SASL development package])
  fi
  CFLAGS="$old_cflags"
  LIBS="$old_libs"
  if test "x$with_sasl2" = "xyes" || test "x$with_sasl" = "xyes" ; then
    AC_DEFINE_UNQUOTED([HAVE_SASL], 1,
      [whether Cyrus SASL is available for authentication])
    enable_sasl=yes
  fi
fi
AM_CONDITIONAL([HAVE_SASL], [test "x$with_sasl2" = "xyes" || test "x$with_sasl" = "xyes"])
AC_SUBST([SASL_CFLAGS])
AC_SUBST([SASL_LIBS])

if test "x$enable_automated_tests" = "xyes"; then
    AC_MSG_CHECKING([for spicy-screenshot])
    spicy-screenshot --help >/dev/null 2>&1
    if test $? -ne 0 ; then
        AC_MSG_RESULT([not found])
        AC_MSG_ERROR([spicy-screenshot was not found, this module is part of spice-gtk and is required to compile this package])
    fi
    AC_MSG_RESULT([found])
fi


AC_ARG_ENABLE([manual],
               AS_HELP_STRING([--enable-manual=@<:@auto/yes/no@:>@],
                              [Build SPICE manual]),
               [],
               [enable_manual="auto"])
if test "x$enable_manual" != "xno"; then
    AC_PATH_PROG([ASCIIDOC], [asciidoc])
    AS_IF([test -z "$ASCIIDOC" && test "x$enable_manual" = "xyes"],
          [AC_MSG_ERROR([asciidoc is missing and build of manual was requested])])
    AC_PATH_PROG([A2X], [a2x])
    AS_IF([test -z "$A2X" && test "x$enable_manual" = "xyes"],
          [AC_MSG_ERROR([a2x is missing and build of manual was requested])])
fi
AS_IF([test -n "$ASCIIDOC"], [have_asciidoc=yes], [have_asciidoc=no])
AM_CONDITIONAL([BUILD_MANUAL], [test -n "$ASCIIDOC" || test -n "$A2X"])
AM_CONDITIONAL([BUILD_HTML_MANUAL], [test -n "$ASCIIDOC"])
AM_CONDITIONAL([BUILD_CHUNKED_MANUAL], [test -n "$A2X"])


dnl ===========================================================================
dnl check compiler flags

SPICE_COMPILE_WARNINGS
LIBVIRT_LINKER_RELRO
LIBVIRT_LINKER_NO_INDIRECT

AC_SUBST(WARN_CFLAGS)

dnl =========================================================================
dnl -fvisibility stuff

have_gcc4=no
AC_MSG_CHECKING(for -fvisibility)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#if defined(__GNUC__) && (__GNUC__ >= 4)
#else
error Need GCC 4.0 for visibility
#endif
int main () { return 0; }
])], have_gcc4=yes)

if test "x$have_gcc4" = "xyes" && test ! "$os_win32" = "yes" ; then
   VISIBILITY_HIDDEN_CFLAGS="-fvisibility=hidden"
fi
AC_MSG_RESULT($have_gcc4)
AC_SUBST(VISIBILITY_HIDDEN_CFLAGS)

dnl ensure linker supports ---version-script option before using it
AC_CACHE_CHECK([if -Wl,--version-script works], [spice_cv_ld_version_script],
    [save_LDFLAGS="$LDFLAGS"
     LDFLAGS="$LDFLAGS -Wl,--version-script=$srcdir/server/spice-server.syms"
     AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
        [spice_cv_ld_version_script=yes], [spice_cv_ld_version_script=no])
     LDFLAGS="$save_LDFLAGS"])
AM_CONDITIONAL([HAVE_LD_VERSION_SCRIPT],
    [test x"$spice_cv_ld_version_script" = xyes])

AC_SUBST(SPICE_REQUIRES)
AC_SUBST(SPICE_NONPKGCONFIG_CFLAGS)
AC_SUBST(SPICE_NONPKGCONFIG_LIBS)

AC_SUBST([SPICEC_STATIC_LINKAGE_BSTATIC])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_OUTPUT([
Makefile
spice-server.pc
server/Makefile
server/spice-version.h
server/tests/Makefile
docs/Makefile
docs/manual/Makefile
])

dnl ==========================================================================
echo "

        Spice $VERSION
        ==============

        prefix:                   ${prefix}
        c compiler:               ${CC}
        c++ compiler:             ${CXX}
        python:                   ${PYTHON}

        OpenGL:                   ${enable_opengl}

        LZ4 support:              ${enable_lz4}

        Smartcard:                ${have_smartcard}

        SASL support:             ${enable_sasl}

        Automated tests:          ${enable_automated_tests}

        Manual:                   ${have_asciidoc}
"

if test x"$arch_warn" != x; then
        AC_MSG_WARN([$arch_warn])
        echo ""
fi

echo \
"        Now type 'make' to build $PACKAGE
"
