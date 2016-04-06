AM_CPPFLAGS = -D_REENTRANT -I$(srcdir)/../libserver
LDADD = $(srcdir)/../libserver/libserver.la -lpthread 

bin_PROGRAMS = client

client_SOURCES = \
	accum.hh accum.cc \
	client.hh client.cc \
	generator.hh \
	gen_memcache.hh gen_memcache.cc \
	gen_synthetic.hh gen_synthetic.cc \
	main.cc \
	opts.hh opts.cc \
	socket.hh socket.cc \
	util.hh util.cc
