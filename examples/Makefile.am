if NEEDS_PTHREAD
STD_THREAD_CXXFLAGS = $(PTHREAD_CFLAGS)
STD_THREAD_LIBS = $(PTHREAD_LIBS)
else
STD_THREAD_CXXFLAGS =
STD_THREAD_LIBS =
endif

AM_CPPFLAGS = -I$(top_srcdir) $(BOOST_CPPFLAGS)
AM_CXXFLAGS = $(STD_THREAD_CXXFLAGS)
AM_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS)

LDADD = ../libpoolqueue.la $(STD_THREAD_LIBS)

BUILT_SOURCES = poolqueue

poolqueue:
	$(LN_S) $(top_srcdir) poolqueue

EXTRA_PROGRAMS = \
	Promise_basics \
	Promise_all \
	Promise_any \
	Promise_close \
	Delay_basics \
	ThreadPool_basics \
	ThreadPool_strand

CLEANFILES = $(EXTRA_PROGRAMS)

Promise_basics_SOURCES = Promise_basics.cpp
Promise_all_SOURCES = Promise_all.cpp
Promise_any_SOURCES = Promise_any.cpp
Promise_close_SOURCES = Promise_close.cpp
Delay_basics_SOURCES = Delay_basics.cpp
ThreadPool_basics_SOURCES = ThreadPool_basics.cpp
ThreadPool_strand_SOURCES = ThreadPool_strand.cpp

if HAS_BOOST_MPI
  AM_LDFLAGS += $(BOOST_MPI_LDFLAGS)
  LDADD += $(BOOST_MPI_LIBS)
endif

if HAS_BOOST_SERIALIZATION
  AM_LDFLAGS += $(BOOST_IOSTREAMS_LDFLAGS) $(BOOST_SERIALIZATION_LDFLAGS)
  LDADD += $(BOOST_IOSTREAMS_LIBS) $(BOOST_SERIALIZATION_LIBS)
endif

.PHONY: examples
examples: $(EXTRA_PROGRAMS)

clean-local:
	-rm -rf poolqueue
