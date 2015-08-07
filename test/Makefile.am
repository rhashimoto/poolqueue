TESTS = Delay_test Promise_test ThreadPool_test MPI_test.sh

AM_CPPFLAGS = -I$(top_srcdir) $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)
LDADD = ../libpoolqueue.la $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

check_PROGRAMS = Delay_test Promise_test ThreadPool_test

Delay_test_SOURCES = Delay_test.cpp
Promise_test_SOURCES = Promise_test.cpp
ThreadPool_test_SOURCES = ThreadPool_test.cpp

if HAS_BOOST_MPI
  AM_LDFLAGS += $(BOOST_MPI_LDFLAGS)
  LDADD += $(BOOST_MPI_LIBS)
  MPI_test_SOURCES = MPI_test.cpp
endif

if HAS_BOOST_SERIALIZATION
  check_PROGRAMS += MPI_test
  AM_LDFLAGS += $(BOOST_IOSTREAMS_LDFLAGS) $(BOOST_SERIALIZATION_LDFLAGS)
  LDADD += $(BOOST_IOSTREAMS_LIBS) $(BOOST_SERIALIZATION_LIBS)
  MPI_test_SOURCES = MPI_test.cpp
endif
