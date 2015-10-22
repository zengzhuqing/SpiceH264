NULL =

AUTOMAKE_OPTIONS = 1.6
DOC_MODULE = spice-gtk

# The top-level SGML file
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.xml

# Source code location
DOC_SOURCE_DIR = $(top_srcdir)/gtk

# Extra options to supply to gtkdoc-scan.
SCAN_OPTIONS = \
	--deprecated-guards="SPICE_DISABLE_DEPRECATED" \
	--ignore-decorators="G_GNUC_INTERNAL"

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS = --xml-mode --output-format=xml

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB = $(top_srcdir)/gtk/*.h
CFILE_GLOB = $(top_srcdir)/gtk/*.c

# Header files to ignore when scanning. Use base file name, no paths
IGNORE_HFILES=					\
	bio-gsocket.h				\
	channel-display-priv.h			\
	channel-usbredir-priv.h			\
	continuation.h				\
	controller				\
	coroutine.h				\
	decode.h				\
	display					\
	gio-coroutine.h				\
	glib-compat.h				\
	smartcard-manager-priv.h		\
	spice-audio-priv.h			\
	spice-channel-cache.h			\
	spice-channel-priv.h			\
	spice-cmdline.h				\
	spice-common.h				\
	spice-gstaudio.h			\
	spice-gtk-session-priv.h		\
	spice-marshal.h				\
	spice-pulse.h				\
	spice-session-priv.h			\
	spice-util-priv.h			\
	spice-widget-priv.h			\
	usb-acl-helper.h			\
	usb-device-manager-priv.h		\
	usbutil.h				\
	vncdisplaykeymap.h			\
	$(NULL)

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
GTKDOC_CFLAGS = -I$(top_srcdir) -I$(top_builddir) -I$(top_srcdir)/gtk -I$(top_builddir)/gtk $(SPICE_GLIB_CFLAGS) $(SPICE_GTK_CFLAGS) $(COMMON_CFLAGS)
GTKDOC_LIBS = $(top_builddir)/gtk/libspice-client-glib-2.0.la $(top_builddir)/gtk/libspice-client-gtk-$(SPICE_GTK_API_VERSION).la

include $(top_srcdir)/gtk-doc.make

# Comment this out if you want 'make check' to test you doc status
# and run some sanity checks
if ENABLE_GTK_DOC
TESTS_ENVIRONMENT = cd $(srcdir) && \
  DOC_MODULE=$(DOC_MODULE) DOC_MAIN_SGML_FILE=$(DOC_MAIN_SGML_FILE) \
  SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir)
#TESTS = $(GTKDOC_CHECK)
endif

-include $(top_srcdir)/git.mk
