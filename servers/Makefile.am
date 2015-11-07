AM_CPPFLAGS = -D_REENTRANT -I$(srcdir)/../libserver
LDADD = $(srcdir)/../libserver/libserver.la -lpthread 

bin_PROGRAMS = pthread libevent isosched golang

pthread_SOURCES = pthread.cc

libevent_SOURCES = libevent.cc
libevent_LDADD = $(LDADD) -levent

isosched_SOURCES = isosched.cc

golang$(EXEEXT): golang.go
	@go build $^
