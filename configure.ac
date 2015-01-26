AC_INIT(poolqueue, 0.1)
AC_CONFIG_SRCDIR([.])
AM_INIT_AUTOMAKE([no-define foreign])

AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/ax_cxx_compile_stdcxx_11.m4])
m4_include([m4/ax_pthread.m4])
m4_include([m4/boost.m4])

AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PROG_LN_S

AX_CXX_COMPILE_STDCXX_11(,mandatory)
AC_LANG(C++)

AC_DEFINE([NDEBUG])

BOOST_REQUIRE([1.48], [echo "'make check' will not work without Boost"])
BOOST_TEST

AM_CONDITIONAL([BOOST_LDPATH], [echo $BOOST_LDPATH | grep -q "[[[:graph:]]]"])

# std::thread requires pthread on Linux.
AX_PTHREAD([true])
AM_CONDITIONAL([NEEDS_PTHREAD], [echo $host_os | grep -q "linux"])

AC_OUTPUT(Makefile)
AC_OUTPUT(test/Makefile)
AC_OUTPUT(examples/Makefile)
