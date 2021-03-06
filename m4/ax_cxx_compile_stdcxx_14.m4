# ============================================================================
#  http://www.gnu.org/software/autoconf-archive/ax_cxx_compile_stdcxx_14.html
# ============================================================================
#
# SYNOPSIS
#
#   AX_CXX_COMPILE_STDCXX_14([ext|noext])
#
# DESCRIPTION
#
#   Check for baseline language coverage in the compiler for the C++14
#   standard; if necessary, add switches to CXXFLAGS to enable support.
#   Errors out if no mode that supports C++14 baseline syntax can be found.
#   The argument, if specified, indicates whether you insist on an extended
#   mode (e.g. -std=gnu++14) or a strict conformance mode (e.g. -std=c++14).
#   If neither is specified, you get whatever works, with preference for an
#   extended mode.
#
# LICENSE
#
#   Copyright (c) 2008 Benjamin Kosnik <bkoz@redhat.com>
#   Copyright (c) 2012 Zack Weinberg <zackw@panix.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 1

m4_define([_AX_CXX_COMPILE_STDCXX_14_testbody], [
  template <typename T>
    struct check
    {
      static_assert(sizeof(int) <= sizeof(T), "not big enough");
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c;
    check_type&& cr = static_cast<check_type&&>(c);
])

AC_DEFUN([AX_CXX_COMPILE_STDCXX_14], [dnl
  m4_if([$1], [], [],
        [$1], [ext], [],
        [$1], [noext], [],
        [m4_fatal([invalid argument `$1' to AX_CXX_COMPILE_STDCXX_14])])dnl
  AC_LANG_ASSERT([C++])dnl
  ac_success=no
  AC_CACHE_CHECK(whether $CXX supports C++14 features by default,
  ax_cv_cxx_compile_cxx14,
  [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_AX_CXX_COMPILE_STDCXX_14_testbody])],
    [ax_cv_cxx_compile_cxx14=yes],
    [ax_cv_cxx_compile_cxx14=no])])
  if test x$ax_cv_cxx_compile_cxx14 = xyes; then
    ac_success=yes
  fi

  m4_if([$1], [noext], [], [dnl
  if test x$ac_success = xno; then
    for switch in -std=gnu++14; do
      cachevar=AS_TR_SH([ax_cv_cxx_compile_cxx14_$switch])
      AC_CACHE_CHECK(whether $CXX supports C++14 features with $switch,
                     $cachevar,
        [ac_save_CXXFLAGS="$CXXFLAGS"
         CXXFLAGS="$CXXFLAGS $switch"
         AC_COMPILE_IFELSE([AC_LANG_SOURCE([_AX_CXX_COMPILE_STDCXX_14_testbody])],
          [eval $cachevar=yes],
          [eval $cachevar=no])
         CXXFLAGS="$ac_save_CXXFLAGS"])
      if eval test x\$$cachevar = xyes; then
        CXXFLAGS="$CXXFLAGS $switch"
        ac_success=yes
        break
      fi
    done
  fi])

  m4_if([$1], [ext], [], [dnl
  if test x$ac_success = xno; then
    for switch in -std=c++14; do
      cachevar=AS_TR_SH([ax_cv_cxx_compile_cxx14_$switch])
      AC_CACHE_CHECK(whether $CXX supports C++14 features with $switch,
                     $cachevar,
        [ac_save_CXXFLAGS="$CXXFLAGS"
         CXXFLAGS="$CXXFLAGS $switch"
         AC_COMPILE_IFELSE([AC_LANG_SOURCE([_AX_CXX_COMPILE_STDCXX_14_testbody])],
          [eval $cachevar=yes],
          [eval $cachevar=no])
         CXXFLAGS="$ac_save_CXXFLAGS"])
      if eval test x\$$cachevar = xyes; then
        CXXFLAGS="$CXXFLAGS $switch"
        ac_success=yes
        break
      fi
    done
  fi])

  if test x$ac_success = xno; then
    AC_MSG_ERROR([*** A compiler with support for C++14 language features is required.])
  fi
])
