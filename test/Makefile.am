TESTS = Delay_test Promise_test ThreadPool_test

AM_CPPFLAGS = -I$(top_srcdir) $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)

LIBS = ../libpoolqueue.la $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

check_PROGRAMS = $(TESTS)

Delay_test_SOURCES = Delay_test.cpp
Promise_test_SOURCES = Promise_test.cpp
ThreadPool_test_SOURCES = ThreadPool_test.cpp
