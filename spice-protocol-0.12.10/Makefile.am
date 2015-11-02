NULL =
ACLOCAL_AMFLAGS = -I m4
SUBDIRS = python_modules spice

codegendir = $(libdir)/spice-protocol
dist_codegen_DATA =			\
	spice.proto			\
	spice1.proto			\
	$(NULL)

dist_codegen_SCRIPTS =			\
	spice_codegen.py		\
	$(NULL)

DISTCLEANFILES = *.pyc

pkgconfigdir = $(datadir)/pkgconfig
pkgconfig_DATA = spice-protocol.pc

MAINTAINERCLEANFILES =						\
	$(srcdir)/INSTALL					\
	$(srcdir)/aclocal.m4					\
	$(srcdir)/autoscan.log					\
	$(srcdir)/compile					\
	$(srcdir)/config.guess					\
	$(srcdir)/config.h.in					\
	$(srcdir)/config.sub					\
	$(srcdir)/configure.scan				\
	$(srcdir)/depcomp					\
	$(srcdir)/install-sh					\
	$(srcdir)/ltmain.sh					\
	$(srcdir)/missing					\
	$(srcdir)/mkinstalldirs					\
	`find "$(srcdir)" -type f -name Makefile.in -print`	\
	$(NULL)

-include $(top_srcdir)/git.mk
