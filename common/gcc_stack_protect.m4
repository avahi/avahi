dnl
dnl Useful macros for autoconf to check for ssp-patched gcc
dnl 1.0 - September 2003 - Tiago Sousa <mirage@kaotik.org>
dnl 1.1 - August 2006 - Ted Percival <ted@midg3t.net>
dnl     * Stricter language checking (C or C++)
dnl     * Adds GCC_STACK_PROTECT_LIB to add -lssp to LDFLAGS as necessary
dnl     * Caches all results
dnl     * Uses macros to ensure correct ouput in quiet/silent mode
dnl
dnl About ssp:
dnl GCC extension for protecting applications from stack-smashing attacks
dnl http://www.research.ibm.com/trl/projects/security/ssp/
dnl
dnl Usage:
dnl Call GCC_STACK_PROTECT_LIB to determine if the library implementing SSP is
dnl available, then the appropriate C or C++ language's test. If you are using
dnl both C and C++ you will need to use AC_LANG_PUSH and AC_LANG_POP to ensure
dnl the right language is being used for each test.
dnl
dnl GCC_STACK_PROTECT_LIB
dnl adds libssp to the LDFLAGS if it is available
dnl
dnl GCC_STACK_PROTECT_CC
dnl checks -fstack-protector with the C compiler, if it exists then updates
dnl CFLAGS and defines ENABLE_SSP_CC
dnl
dnl GCC_STACK_PROTECT_CXX
dnl checks -fstack-protector with the C++ compiler, if it exists then updates
dnl CXXFLAGS and defines ENABLE_SSP_CXX
dnl

AC_DEFUN([GCC_STACK_PROTECT_LIB],[
  AC_CACHE_CHECK([whether libssp exists], ssp_cv_lib,
    [ssp_old_ldflags="$LDFLAGS"
     LDFLAGS="$LDFLAGS -lssp"
     AC_TRY_LINK(,, ssp_cv_lib=yes, ssp_cv_lib=no)
     LDFLAGS="$ssp_old_ldflags"
    ])
  if test $ssp_cv_lib = yes; then
    LDFLAGS="$LDFLAGS -lssp"
  fi
])

AC_DEFUN([GCC_STACK_PROTECT_CC],[
  AC_LANG_ASSERT(C)
  if test "X$CC" != "X"; then
    AC_CACHE_CHECK([whether ${CC} accepts -fstack-protector],
      ssp_cv_cc,
      [ssp_old_cflags="$CFLAGS"
       CFLAGS="$CFLAGS -fstack-protector"
       AC_TRY_COMPILE(,, ssp_cv_cc=yes, ssp_cv_cc=no)
       CFLAGS="$ssp_old_cflags"
      ])
    if test $ssp_cv_cc = yes; then
      CFLAGS="$CFLAGS -fstack-protector"
      AC_DEFINE([ENABLE_SSP_CC], 1, [Define if SSP C support is enabled.])
    fi
  fi
])

AC_DEFUN([GCC_STACK_PROTECT_CXX],[
  AC_LANG_ASSERT(C++)
  if test "X$CXX" != "X"; then
    AC_CACHE_CHECK([whether ${CXX} accepts -fstack-protector],
      ssp_cv_cxx,
      [ssp_old_cxxflags="$CXXFLAGS"
       CXXFLAGS="$CXXFLAGS -fstack-protector"
       AC_TRY_COMPILE(,, ssp_cv_cxx=yes, ssp_cv_cxx=no)
       CXXFLAGS="$ssp_old_cxxflags"
      ])
    if test $ssp_cv_cxx = yes; then
      CXXFLAGS="$CXXFLAGS -fstack-protector"
      AC_DEFINE([ENABLE_SSP_CXX], 1, [Define if SSP C++ support is enabled.])
    fi
  fi
])

