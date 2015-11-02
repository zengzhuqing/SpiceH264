# For autoconf < 2.63
m4_ifndef([AS_VAR_APPEND],
          AC_DEFUN([AS_VAR_APPEND], $1=$$1$2))


# SPICE_CHECK_SYSDEPS()
# ---------------------
# Checks for header files and library functions needed by spice-common.
# ---------------------
AC_DEFUN([SPICE_CHECK_SYSDEPS], [
    AC_C_BIGENDIAN
    AC_FUNC_ALLOCA
    AC_CHECK_HEADERS([arpa/inet.h malloc.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/socket.h unistd.h])

    # Checks for typedefs, structures, and compiler characteristics
    AC_C_INLINE
    AC_TYPE_INT16_T
    AC_TYPE_INT32_T
    AC_TYPE_INT64_T
    AC_TYPE_INT8_T
    AC_TYPE_PID_T
    AC_TYPE_SIZE_T
    AC_TYPE_UINT16_T
    AC_TYPE_UINT32_T
    AC_TYPE_UINT64_T
    AC_TYPE_UINT8_T

    # Checks for library functions
    # do not check malloc or realloc, since that cannot be cross-compiled checked
    AC_FUNC_ERROR_AT_LINE
    AC_FUNC_FORK
    AC_CHECK_FUNCS([dup2 floor inet_ntoa memmove memset pow sqrt])
])


# SPICE_CHECK_SMARTCARD(PREFIX)
# -----------------------------
# Adds a --enable-smartcard switch in order to enable/disable smartcard
# support, and checks if the needed libraries are available. If found, it will
# append the flags to use to the $PREFIX_CFLAGS and $PREFIX_LIBS variables, and
# it will define a USE_SMARTCARD preprocessor symbol.
#------------------------------
AC_DEFUN([SPICE_CHECK_SMARTCARD], [
    AC_ARG_ENABLE([smartcard],
      AS_HELP_STRING([--enable-smartcard=@<:@yes/no/auto@:>@],
                     [Enable smartcard support @<:@default=auto@:>@]),
      [],
      [enable_smartcard="auto"])

    have_smartcard=no
    if test "x$enable_smartcard" != "xno"; then
      PKG_CHECK_MODULES([SMARTCARD], [libcacard >= 0.1.2], [have_smartcard=yes], [have_smartcard=no])
      if test "x$enable_smartcard" != "xauto" && test "x$have_smartcard" = "xno"; then
        AC_MSG_ERROR("Smartcard support requested but libcacard could not be found")
      fi
      if test "x$have_smartcard" = "xyes"; then
        AC_DEFINE(USE_SMARTCARD, [1], [Define if supporting smartcard proxying])
      fi
    fi
    AS_VAR_APPEND([$1_CFLAGS], [" $SMARTCARD_CFLAGS"])
    AS_VAR_APPEND([$1_LIBS], [" $SMARTCARD_LIBS"])
])


# SPICE_CHECK_CELT051(PREFIX)
# ---------------------------
# Adds a --disable-celt051 switch in order to enable/disable CELT 0.5.1
# support, and checks if the needed libraries are available. If found, it will
# append the flags to use to the $PREFIX_CFLAGS and $PREFIX_LIBS variables, and
# it will define a HAVE_CELT051 preprocessor symbol as well as a HAVE_CELT051
# Makefile conditional.
#----------------------------
AC_DEFUN([SPICE_CHECK_CELT051], [
    AC_ARG_ENABLE([celt051],
        [  --disable-celt051       Disable celt051 audio codec (enabled by default)],,
        [enable_celt051="yes"])

    if test "x$enable_celt051" = "xyes"; then
        PKG_CHECK_MODULES([CELT051], [celt051 >= 0.5.1.1], [have_celt051=yes], [have_celt051=no])
    else
        have_celt051=no
    fi

    AM_CONDITIONAL([HAVE_CELT051], [test "x$have_celt051" = "xyes"])
    AM_COND_IF([HAVE_CELT051], AC_DEFINE([HAVE_CELT051], 1, [Define if we have celt051 codec]))
    AS_VAR_APPEND([$1_CFLAGS], [" $CELT051_CFLAGS"])
    AS_VAR_APPEND([$1_LIBS], [" $CELT051_LIBS"])
])


# SPICE_CHECK_OPUS(PREFIX)
# ------------------------
# Check for the availability of Opus. If found, it will append the flags to use
# to the $PREFIX_CFLAGS and $PREFIX_LIBS variables, and it will define a
# HAVE_OPUS preprocessor symbol as well as a HAVE_OPUS Makefile conditional.
#-------------------------
AC_DEFUN([SPICE_CHECK_OPUS], [
    PKG_CHECK_MODULES([OPUS], [opus >= 0.9.14], [have_opus=yes], [have_opus=no])

    AM_CONDITIONAL([HAVE_OPUS], [test "x$have_opus" = "xyes"])
    if test "x$have_opus" = "xyes" ; then
      AC_DEFINE([HAVE_OPUS], [1], [Define if we have OPUS])
    fi
    AS_VAR_APPEND([$1_CFLAGS], [" $OPUS_CFLAGS"])
    AS_VAR_APPEND([$1_LIBS], [" $OPUS_LIBS"])
])


# SPICE_CHECK_OPENGL(PREFIX)
# --------------------------
# Adds a --disable-opengl switch in order to enable/disable OpenGL
# support, and checks if the needed libraries are available. If found, it will
# append the flags to use to the $PREFIX_CFLAGS and $PREFIX_LIBS variables, and
# it will define USE_OPENGL and GL_GLEXT_PROTOTYPES preprocessor symbol as well
# as a SUPPORT_GL Makefile conditional.
#---------------------------
AC_DEFUN([SPICE_CHECK_OPENGL], [
    AC_ARG_ENABLE([opengl],
        AS_HELP_STRING([--enable-opengl=@<:@yes/no@:>@],
                       [Enable opengl support (not recommended) @<:@default=no@:>@]),
        [],
        [enable_opengl="no"])
    AM_CONDITIONAL(SUPPORT_GL, test "x$enable_opengl" = "xyes")

    if test "x$enable_opengl" = "xyes"; then
        AC_CHECK_LIB(GL, glBlendFunc, GL_LIBS="$GL_LIBS -lGL", enable_opengl=no)
        AC_CHECK_LIB(GLU, gluSphere, GL_LIBS="$GL_LIBS -lGLU", enable_opengl=no)
        AC_DEFINE([USE_OPENGL], [1], [Define to build with OpenGL support])
        AC_DEFINE([GL_GLEXT_PROTOTYPES], [], [Enable GLExt prototypes])

        if test "x$enable_opengl" = "xno"; then
            AC_MSG_ERROR([GL libraries not available])
        fi
    fi
    AS_VAR_APPEND([$1_CFLAGS], [" $GL_CFLAGS"])
    AS_VAR_APPEND([$1_LIBS], [" $GL_LIBS"])
])


# SPICE_CHECK_PIXMAN(PREFIX)
# --------------------------
# Check for the availability of pixman. If found, it will append the flags to
# use to the $PREFIX_CFLAGS and $PREFIX_LIBS variables.
#---------------------------
AC_DEFUN([SPICE_CHECK_PIXMAN], [
    PKG_CHECK_MODULES(PIXMAN, pixman-1 >= 0.17.7)
    AS_VAR_APPEND([$1_CFLAGS], [" $PIXMAN_CFLAGS"])
    AS_VAR_APPEND([$1_LIBS], [" $PIXMAN_LIBS"])
])


# SPICE_CHECK_GLIB2(PREFIX)
# --------------------------
# Check for the availability of glib2. If found, it will append the flags to
# use to the $PREFIX_CFLAGS and $PREFIX_LIBS variables.
#---------------------------
AC_DEFUN([SPICE_CHECK_GLIB2], [
    PKG_CHECK_MODULES(GLIB2, glib-2.0)
    AS_VAR_APPEND([$1_CFLAGS], [" $GLIB2_CFLAGS"])
    AS_VAR_APPEND([$1_LIBS], [" $GLIB2_LIBS"])
])

# SPICE_CHECK_PYTHON_MODULES()
# --------------------------
# Adds a --enable-python-checks configure flags as well as checks for the
# availability of the python modules needed by the python scripts generating
# C code from spice.proto. These checks are not needed when building from
# tarballs so they are disabled by default.
#---------------------------
AC_DEFUN([SPICE_CHECK_PYTHON_MODULES], [
    AM_PATH_PYTHON
    AC_ARG_ENABLE([python-checks],
        AS_HELP_STRING([--enable-python-checks=@<:@yes/no@:>@],
                       [Enable checks for Python modules needed to build from git @<:@default=no@:>@]),
                       [],
                       [enable_python_checks="no"])
    if test "x$enable_python_checks" != "xno"; then
        AX_PYTHON_MODULE([six], [1])
        AX_PYTHON_MODULE([pyparsing], [1])
    fi
])


# SPICE_CHECK_LZ4(PREFIX)
# -----------------------------
# Adds a --enable-lz4 switch in order to enable/disable LZ4 compression
# support, and checks if the needed libraries are available. If found, it will
# append the flags to use to the $PREFIX_CFLAGS and $PREFIX_LIBS variables, and
# it will define a USE_LZ4 preprocessor symbol as well as a SUPPORT_LZ4 Makefile
# conditional.
#------------------------------
AC_DEFUN([SPICE_CHECK_LZ4], [
    AC_ARG_ENABLE([lz4],
      AS_HELP_STRING([--enable-lz4=@<:@yes/no@:>@],
                     [Enable LZ4 compression support @<:@default=no@:>@]),
      [],
      [enable_lz4="no"])

    if test "x$enable_lz4" != "xno"; then
      PKG_CHECK_MODULES([LZ4], [liblz4])
      AC_DEFINE(USE_LZ4, [1], [Define to build with lz4 support])
    fi
    AS_VAR_APPEND([$1_CFLAGS], [" $LZ4_CFLAGS"])
    AS_VAR_APPEND([$1_LIBS], [" $LZ4_LIBS"])
])
