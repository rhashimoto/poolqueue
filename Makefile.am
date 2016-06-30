ACLOCAL_AMFLAGS = -I m4
AM_CXXFLAGS= -Wall -Wextra -Werror

if NEEDS_PTHREAD
  AM_CXXFLAGS += $(PTHREAD_CFLAGS)
endif

SUBDIRS = . test examples

lib_LTLIBRARIES = libpoolqueue.la
libpoolqueue_la_CXXFLAGS=$(AM_CXXFLAGS) $(DEBUGFLAGS)
libpoolqueue_la_SOURCES = Promise.cpp Delay.cpp

otherincludedir = $(includedir)/poolqueue
otherinclude_HEADERS = Promise.hpp Promise_detail.hpp Delay.hpp ThreadPool.hpp ThreadPool_detail.hpp

if HAS_BOOST_SERIALIZATION
  libpoolqueue_la_SOURCES += MPI.cpp
  otherinclude_HEADERS += MPI.hpp
endif

EXTRA_DIST = NOTICE
