#  bin_PROGRAMS = IndriPruneIndex
#  IndriPruneIndex_SOURCES = IndriPruneIndex.cpp diagnosis.cpp utils.cpp
bin_PROGRAMS = IndriJSONServer
IndriJSONServer_SOURCES = IndriJSONServer.cpp
IndriJSONServer_LDADD = $(BOOST_SYSTEM_LIB) -lcppnetlib-uri

noinst_LIBRARIES = libmain.a
libmain_a_SOURCES = $(IndriJSONServer_SOURCES)
