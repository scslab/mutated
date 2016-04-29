AM_CPPFLAGS = -D_REENTRANT -I$(srcdir)/../libserver
LDADD = $(srcdir)/../libserver/libserver.la -lpthread 

if LIBEVENTM_OPT
    MAYBE_LIBEVENTM = libeventm
endif

bin_PROGRAMS = pthread isosched golang libevent $(MAYBE_LIBEVENTM)
EXTRA_PROGRAMS = libeventm

pthread_SOURCES = pthread.cc

isosched_SOURCES = isosched.cc

golang_SOURCES = golang.go

golang$(EXEEXT): golang.go
	@go build $^

libevent_SOURCES = libevent.cc
libevent_LDADD = $(LDADD) -levent

libeventm_SOURCES = libeventm.cc
libeventm_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/../libeventm/include
libeventm_LDADD = $(LDADD) $(srcdir)/../libeventm/libevent.la

