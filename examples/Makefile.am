AM_CPPFLAGS = -I$(top_srcdir) $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS)

# if BOOST_LDPATH
# AM_LDFLAGS += -L$(BOOST_LDPATH)
# endif

LIBS = ../libpoolqueue.la

BUILT_SOURCES = poolqueue

poolqueue:
	$(LN_S) $(top_srcdir) poolqueue

EXTRA_PROGRAMS = foo
CLEANFILES = $(EXTRA_PROGRAMS)

foo_SOURCES = foo.cpp

.PHONY: examples
examples: $(EXTRA_PROGRAMS)

clean-local:
	-rm -rf poolqueue
