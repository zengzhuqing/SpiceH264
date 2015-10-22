ACLOCAL_AMFLAGS = -I m4
NULL =

SUBDIRS = spice-common gtk po doc data tests

if HAVE_INTROSPECTION
if WITH_VALA
SUBDIRS += vapi
endif
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =					\
	spice-client-glib-2.0.pc			\
	spice-client-gtk-$(SPICE_GTK_API_VERSION).pc	\
	$(NULL)

if WITH_CONTROLLER
pkgconfig_DATA += spice-controller.pc
endif

INTLTOOL_FILES =				\
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	$(NULL)

DISTCLEANFILES =				\
	$(pkgconfig_DATA)			\
	intltool-extract			\
	intltool-merge				\
	intltool-update				\
	gnome-doc-utils.make			\
	po/.intltool-merge-cache		\
	$(NULL)

EXTRA_DIST =					\
	build-aux/git-version-gen		\
	gtk-doc.make				\
	.version				\
	$(INTLTOOL_FILES)			\
	$(NULL)

MAINTAINERCLEANFILES =				\
	ABOUT-NLS				\
	aclocal.m4				\
	config.h.in				\
	m4/gtk-doc.m4				\
	m4/intltool.m4				\
	m4/libtool.m4				\
	m4/ltoptions.m4				\
	m4/ltsugar.m4				\
	m4/ltversion.m4				\
	m4/lt~obsolete.m4			\
	build-aux/compile			\
	build-aux/config.guess			\
	build-aux/config.rpath			\
	build-aux/config.sub			\
	build-aux/depcomp			\
	build-aux/install-sh			\
	build-aux/ltmain.sh			\
	build-aux/missing			\
	build-aux/mkinstalldirs			\
	po/Makefile.in.in			\
	gtk-doc.make				\
	$(NULL)

# Generate the ChangeLog file
# and insert it into the directory we're about to use to create a tarball.
.PHONY: gen-ChangeLog gen-THANKS
gen-ChangeLog:
	if test -d .git || test -d ../.git; then				\
	  $(top_srcdir)/build-aux/gitlog-to-changelog > $(distdir)/cl-t;	\
	  rm -f $(distdir)/ChangeLog;						\
	  mv $(distdir)/cl-t $(distdir)/ChangeLog;				\
	fi

# see git-version-gen
dist-hook: gen-ChangeLog gen-THANKS
	echo $(VERSION) > $(distdir)/.tarball-version

BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@

DISTCHECK_CONFIGURE_FLAGS =			\
	--enable-introspection			\
	--disable-vala				\
	--disable-usbredir			\
	--enable-gtk-doc			\
	--enable-werror				\
	--with-gtk=3.0				\
	$(NULL)

gen-THANKS:
	$(AM_V_GEN)if test -d .git || test -d ../.git; then						\
	  echo "The spice-gtk team would like to thank the following contributors:" > $(distdir)/t-t;	\
	  echo >> $(distdir)/t-t;									\
	  git log --format='%aN <%aE>' | sort -u >> $(distdir)/t-t;					\
	  rm -f $(distdir)/THANKS;									\
	  mv $(distdir)/t-t $(distdir)/THANKS;								\
	fi

-include $(top_srcdir)/git.mk
