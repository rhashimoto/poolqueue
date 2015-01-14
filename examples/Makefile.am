AM_CXXFLAGS = $(BOOST_CPPFLAGS)
AM_LDFLAGS = -L$(libdir)

if BOOST_LDPATH
AM_LDFLAGS += -L$(BOOST_LDPATH)
endif

TARGETS = foo

$(TARGETS) : % : %.cpp
	$(CXX) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) -o $@ $^ $(AM_LDFLAGS) $(LDFLAGS) $(LIBS)

clean-local:
	-rm -rf $(TARGETS) *.dSYM
