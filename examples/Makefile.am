AM_CPPFLAGS = -I$(top_srcdir) $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS)

# if BOOST_LDPATH
# AM_LDFLAGS += -L$(BOOST_LDPATH)
# endif

LIBS = ../libpoolqueue.la

BUILT_SOURCES = poolqueue

poolqueue:
	$(LN_S) $(top_srcdir) poolqueue

EXTRA_PROGRAMS = \
	Promise_basics \
	Promise_all \
	Promise_close \
	ThreadPool_strand

CLEANFILES = $(EXTRA_PROGRAMS)

Promise_basics_SOURCES = Promise_basics.cpp
Promise_all_SOURCES = Promise_all.cpp
Promise_close_SOURCES = Promise_close.cpp
ThreadPool_strand_SOURCES = ThreadPool_strand.cpp

.PHONY: examples
examples: $(EXTRA_PROGRAMS)

clean-local:
	-rm -rf poolqueue
