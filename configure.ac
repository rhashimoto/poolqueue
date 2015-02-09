AC_INIT(poolqueue, 0.1.0)
AC_CONFIG_SRCDIR([.])
AM_INIT_AUTOMAKE([no-define foreign])

AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/ax_cxx_compile_stdcxx_11.m4])
m4_include([m4/ax_pthread.m4])
m4_include([m4/boost.m4])
m4_include([m4/ax_prog_cxx_mpi.m4])
m4_include([m4/ax_check_zlib.m4])

# If --with-mpi=auto is used, try to find MPI, but use standard C compiler if it is not found.
# If --with-mpi=yes is used, try to find MPI and fail if it isn't found.
# If --with-mpi=no is used, use a standard C compiler instead.
AC_ARG_WITH(mpi, [AS_HELP_STRING([--with-mpi],
    [compile with MPI (parallelization) support. If none is found,
    MPI is not used. Default: auto])
],,[with_mpi=auto])

AX_PROG_CXX_MPI([test x"$with_mpi" != xno],[use_mpi=yes],[
  use_mpi=no
  if test x"$with_mpi" = xyes; then
    AC_MSG_FAILURE([MPI compiler requested, but couldn't use MPI.])
  else
    AC_MSG_WARN([No MPI compiler found, won't use MPI.])
  fi
])
AC_PROG_LIBTOOL
AC_PROG_LN_S

AX_CXX_COMPILE_STDCXX_11(,mandatory)
AC_LANG(C++)

AC_DEFINE([NDEBUG])

BOOST_REQUIRE([1.48], [echo "'make check' will not work without Boost"])
AS_IF([test x"$use_mpi" = xyes], [
  BOOST_MPI
])
BOOST_IOSTREAMS
BOOST_SERIALIZATION
BOOST_TEST
AX_CHECK_ZLIB

AM_CONDITIONAL([BOOST_LDPATH], [test -n "$BOOST_LDPATH"])

AM_CONDITIONAL([HAS_BOOST_MPI], [test -n "$BOOST_MPI_LIBS"])
AM_CONDITIONAL([HAS_BOOST_SERIALIZATION], [test -n "$BOOST_SERIALIZATION_LIBS"])

# std::thread requires pthread on Linux.
AX_PTHREAD([true])
AM_CONDITIONAL([NEEDS_PTHREAD], [echo $host_os | grep -q "linux"])

AC_OUTPUT(Makefile)
AC_OUTPUT(test/Makefile)
AC_OUTPUT(examples/Makefile)
