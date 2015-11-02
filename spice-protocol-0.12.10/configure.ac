AC_PREREQ([2.57])

m4_define([SPICE_MAJOR], 0)
m4_define([SPICE_MINOR], 12)
m4_define([SPICE_MICRO], 10)

AC_INIT(spice-protocol, [SPICE_MAJOR.SPICE_MINOR.SPICE_MICRO], [], spice-protocol)

AC_CONFIG_MACRO_DIR([m4])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl Enable generation of enums.h, mainly useful when building from git,
dnl should not be needed for tarball users
AC_ARG_ENABLE([code-generator],
    AS_HELP_STRING([--enable-code-generator=@<:@yes/no/auto@:>@],
                       [Enable checks needed to use the Python code generator @<:@default=auto@:>@]),
                       [],
                       [enable_code_generator="auto"])
if test "x$enable_code_generator" != "xno"; then
    AS_IF([test "x$enable_code_generator" = "xyes"], modules_required=[1], modules_required=[])
    AX_PYTHON_MODULE([six], $modules_required)
    AX_PYTHON_MODULE([pyparsing], $modules_required)
    AS_IF([test "x${HAVE_PYMOD_SIX}" = "xno"], enable_code_generator="no")
    AS_IF([test "x${HAVE_PYMOD_PYPARSING}" = "xno"], enable_code_generator="no")
fi
AM_CONDITIONAL([ENABLE_CODEGEN], [test "x$enable_code_generator" != "xno"])

AC_OUTPUT([
Makefile
spice-protocol.pc
spice/Makefile
python_modules/Makefile
])
