ACLOCAL_AMFLAGS = -I m4
AM_CXXFLAGS= -Wall -Wextra -Werror

SUBDIRS = . test examples

lib_LTLIBRARIES = libpoolqueue.la
libpoolqueue_la_CXXFLAGS=$(AM_CXXFLAGS) $(DEBUGFLAGS)
libpoolqueue_la_SOURCES = Promise.cpp Delay.cpp ThreadPool.cpp

otherincludedir = $(includedir)/poolqueue
otherinclude_HEADERS = Promise.hpp Promise_detail.hpp Delay.hpp ThreadPool.hpp

EXTRA_DIST = NOTICE
