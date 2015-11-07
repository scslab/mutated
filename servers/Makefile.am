AM_CPPFLAGS = -D_REENTRANT -I$(srcdir)/../libserver
LDADD = $(srcdir)/../libserver/libserver.la -lpthread 

bin_PROGRAMS = pthread libevent libeventm isosched golang

pthread_SOURCES = pthread.cc

libevent_SOURCES = libevent.cc
libevent_LDADD = $(LDADD) -levent

libeventm_SOURCES = libeventm.cc
libeventm_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/../libeventm/include
libeventm_LDADD = $(LDADD) $(srcdir)/../libeventm/libevent.la

isosched_SOURCES = isosched.cc

golang$(EXEEXT): golang.go
	@go build $^
