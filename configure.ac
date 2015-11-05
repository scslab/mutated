# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([project],[1.0.0],[dterei@cs.stanford.edu])

# Set automake options.
# `-Wall -Werror` here is for automake warning, nothing to do with src
# compilation.
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Silent build please
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Set sanity check for a source file.
AC_CONFIG_SRCDIR([bin/app.cc])

# Set m4 directory.
AC_CONFIG_MACRO_DIR([m4])

# Set configure output header.
AC_CONFIG_HEADERS([config.h])

# Set some default CXXFLAGS
WARN_FLAGS="-pedantic -Wall -Wextra -Weffc++"
AC_SUBST([AM_CXXFLAGS], ["$WARN_FLAGS"])

# We set directly (default is 'cru' as 'u' option produces a warning since it's
# ignored when using deterministic libraries).
AC_SUBST([AR_FLAGS], ["cr"])

# Checks for programs.
AC_PROG_CXX
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_11([noext])
AM_PROG_AR
LT_INIT

# Checks for header files.
# AC_CHECK_HEADERS([<header file>])

# Checks for typedefs, structures, and compiler characteristics.
# AC_TYPE_UINT16_T
# AC_TYPE_UINT32_T
# AC_TYPE_UINT64_T

# Checks for library functions.

# Set output files.
AC_CONFIG_FILES([
  Makefile
  bin/Makefile
  libapp/Makefile
])
AC_OUTPUT
