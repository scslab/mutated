AM_CPPFLAGS = -D_REENTRANT -I$(srcdir)/../libserver
LDADD = -lpthread

bin_PROGRAMS = memcacheload client test1

client_LDADD = $(srcdir)/../libserver/libserver.la
client_SOURCES = \
	accum.hh accum.cc \
	client.hh client.cc \
	generator.hh \
	gen_synthetic.hh gen_synthetic.cc \
	gen_memcache.hh gen_memcache.cc \
	main.cc \
	opts.hh opts.cc \
	socket_buf.hh socket_buf.cc \
	util.hh

# We don't compile the following files:
#   socket_vec.hh socket_vec.cc
# as they provide an older, conflicting implementation of the Sock class. We
# leave present though as it's still useful code.

memcacheload_SOURCES = \
    memcacheload.cc \
	socket_buf.hh socket_buf.cc \
	util.hh

test1_SOURCES = test1.cc
