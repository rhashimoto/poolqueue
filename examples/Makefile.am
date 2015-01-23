AM_CPPFLAGS = -I$(top_srcdir) $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS)

# if BOOST_LDPATH
# AM_LDFLAGS += -L$(BOOST_LDPATH)
# endif

LIBS = ../libpoolqueue.la

BUILT_SOURCES = poolqueue

poolqueue:
	$(LN_S) $(top_srcdir) poolqueue

EXTRA_PROGRAMS = Promise_basics Promise_chain Promise_sub
CLEANFILES = $(EXTRA_PROGRAMS)

Promise_basics_SOURCES = Promise_basics.cpp
Promise_chain_SOURCES = Promise_chain.cpp
Promise_sub_SOURCES = Promise_sub.cpp

.PHONY: examples
examples: $(EXTRA_PROGRAMS)

clean-local:
	-rm -rf poolqueue
