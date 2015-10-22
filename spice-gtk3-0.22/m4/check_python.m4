# serial 3
# Find valid warning flags for the C Compiler.           -*-Autoconf-*-
#
# Copyright (C) 2001, 2002, 2006 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA

# Written by Jesse Thilo.

dnl a macro to check for ability to create python extensions
dnl  AM_CHECK_PYTHON_HEADERS([ACTION-IF-POSSIBLE], [ACTION-IF-NOT-POSSIBLE])
dnl function also defines PYTHON_INCLUDES
AC_DEFUN([AM_CHECK_PYTHON_HEADERS],
  [AC_REQUIRE([AM_PATH_PYTHON])
    AC_MSG_CHECKING(for headers required to compile python extensions)
    dnl deduce PYTHON_INCLUDES
    py_prefix=`$PYTHON -c "import sys; print sys.prefix"`
    py_exec_prefix=`$PYTHON -c "import sys; print sys.exec_prefix"`
    PYTHON_INCLUDES="-I${py_prefix}/include/python${PYTHON_VERSION}"
    if test "$py_prefix" != "$py_exec_prefix"; then
       PYTHON_INCLUDES="$PYTHON_INCLUDES -I${py_exec_prefix}/include/python${PYTHON_VERSION}"
    fi
    AC_SUBST(PYTHON_INCLUDES)
    dnl check if the headers exist:
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $PYTHON_INCLUDES"
    AC_TRY_CPP([#include <Python.h>],dnl
    [AC_MSG_RESULT(found)
       $1],dnl
       [AC_MSG_RESULT(not found)
       $2])
    CPPFLAGS="$save_CPPFLAGS"
])
