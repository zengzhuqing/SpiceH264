NULL =

noinst_PROGRAMS =				\
	coroutine				\
	util					\
	session					\
	$(NULL)

if WITH_PHODAV
noinst_PROGRAMS += pipe
endif

TESTS = $(noinst_PROGRAMS)

AM_CPPFLAGS =					\
	$(GIO_CFLAGS)				\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	-DG_LOG_DOMAIN=\"GSpice\"		\
	$(NULL)

AM_LDFLAGS = $(GIO_LIBS) -static

LDADD =							\
	$(top_builddir)/src/libspice-client-glib-2.0.la	\
	$(NULL)

util_SOURCES = util.c
coroutine_SOURCES = coroutine.c
session_SOURCES = session.c
pipe_SOURCES = pipe.c


-include $(top_srcdir)/git.mk
