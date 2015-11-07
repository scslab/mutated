AM_CPPFLAGS = -D_REENTRANT -I$(srcdir)/../libserver
LDADD = $(srcdir)/../libserver/libserver.la -lpthread 

bin_PROGRAMS = client

client_SOURCES = \
	accum.hh accum.cc \
	client.cc \
	generator.hh generator.cc \
	socket.hh socket.cc
