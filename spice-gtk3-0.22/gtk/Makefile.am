NULL =

SUBDIRS =

if WITH_CONTROLLER
SUBDIRS += controller
endif

# Avoid need for perl(Text::CSV) by end users
KEYMAPS =					\
	vncdisplaykeymap_xorgevdev2xtkbd.c	\
	vncdisplaykeymap_xorgkbd2xtkbd.c	\
	vncdisplaykeymap_xorgxquartz2xtkbd.c	\
	vncdisplaykeymap_xorgxwin2xtkbd.c	\
	vncdisplaykeymap_osx2xtkbd.c		\
	vncdisplaykeymap_win322xtkbd.c		\
	vncdisplaykeymap_x112xtkbd.c		\
	$(NULL)

# End users build dependencies can be cleaned
GLIBGENS =					\
	spice-glib-enums.c			\
	spice-glib-enums.h			\
	spice-marshal.c				\
	spice-marshal.h				\
	spice-widget-enums.c			\
	spice-widget-enums.h			\
	$(NULL)

CLEANFILES = $(GLIBGENS)
BUILT_SOURCES = $(GLIBGENS) $(KEYMAPS)

EXTRA_DIST =					\
	$(KEYMAPS)				\
	decode-glz-tmpl.c			\
	keymap-gen.pl				\
	keymaps.csv				\
	map-file				\
	spice-glib-sym-file			\
	spice-gtk-sym-file			\
	spice-client-gtk-manual.defs		\
	spice-client-gtk.override		\
	spice-marshal.txt			\
	$(NULL)

bin_PROGRAMS = spicy spicy-stats spicy-screenshot
if WITH_POLKIT
acldir = $(ACL_HELPER_DIR)
acl_PROGRAMS = spice-client-glib-usb-acl-helper
endif

lib_LTLIBRARIES = libspice-client-glib-2.0.la

if HAVE_GTK_2
lib_LTLIBRARIES += libspice-client-gtk-2.0.la
else
lib_LTLIBRARIES += libspice-client-gtk-3.0.la
endif

if HAVE_LD_VERSION_SCRIPT
GLIB_VERSION_LDFLAGS = -Wl,--version-script=${srcdir}/map-file
GTK_VERSION_LDFLAGS = $(GLIB_VERSION_LDFLAGS)
else
GLIB_VERSION_LDFLAGS = -export-symbols ${srcdir}/spice-glib-sym-file
GTK_VERSION_LDFLAGS = -export-symbols ${srcdir}/spice-gtk-sym-file
endif

KEYMAP_GEN = $(srcdir)/keymap-gen.pl

SPICE_COMMON_CPPFLAGS =						\
	-DG_LOG_DOMAIN=\"GSpice\"				\
	-DSPICE_NO_DEPRECATED					\
	-DSW_CANVAS_CACHE					\
	-DSPICE_GTK_LOCALEDIR=\"${SPICE_GTK_LOCALEDIR}\"	\
	-DPNP_IDS=\""$(PNP_IDS)"\"				\
	-DUSB_IDS=\""$(USB_IDS)"\"				\
	-DSPICE_DISABLE_ABORT					\
	-I$(top_srcdir)						\
	$(COMMON_CFLAGS)					\
	$(PIXMAN_CFLAGS)					\
	$(CELT051_CFLAGS)					\
	$(PULSE_CFLAGS)						\
	$(GTK_CFLAGS)						\
	$(CAIRO_CFLAGS)						\
	$(GLIB2_CFLAGS)						\
	$(GIO_CFLAGS)						\
	$(GOBJECT2_CFLAGS)					\
	$(DBUS_GLIB_CFLAGS)					\
	$(SSL_CFLAGS)						\
	$(SASL_CFLAGS)						\
	$(GST_CFLAGS)						\
	$(SMARTCARD_CFLAGS)					\
	$(USBREDIR_CFLAGS)					\
	$(GUDEV_CFLAGS)						\
	$(NULL)

AM_CPPFLAGS =					\
	$(SPICE_COMMON_CPPFLAGS)		\
	$(SPICE_CFLAGS)				\
	$(NULL)

# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
SPICE_GTK_LDFLAGS_COMMON =		\
	-version-info 4:0:0		\
	-no-undefined			\
	$(GTK_VERSION_LDFLAGS)		\
	$(NULL)

SPICE_GTK_LIBADD_COMMON =		\
	libspice-client-glib-2.0.la	\
	$(GTK_LIBS)			\
	$(CAIRO_LIBS)			\
	$(DBUS_GLIB_LIBS)		\
	$(XRANDR_LIBS)			\
	$(LIBM)				\
	$(NULL)

SPICE_GTK_SOURCES_COMMON =		\
	glib-compat.h			\
	spice-util.c			\
	spice-util-priv.h		\
	spice-gtk-session.c		\
	spice-gtk-session-priv.h	\
	spice-widget.c			\
	spice-widget-priv.h		\
	vncdisplaykeymap.c		\
	vncdisplaykeymap.h		\
	spice-grabsequence.c		\
	spice-grabsequence.h		\
	desktop-integration.c		\
	desktop-integration.h		\
	usb-device-widget.c		\
	$(NULL)

nodist_SPICE_GTK_SOURCES_COMMON =	\
	spice-widget-enums.c		\
	spice-marshal.c			\
	$(NULL)

if WITH_X11
SPICE_GTK_SOURCES_COMMON +=		\
	spice-widget-x11.c		\
	$(NULL)
else
SPICE_GTK_SOURCES_COMMON +=		\
	spice-widget-cairo.c		\
	$(NULL)
endif

if HAVE_GTK_2
libspice_client_gtk_2_0_la_LDFLAGS = $(SPICE_GTK_LDFLAGS_COMMON)
libspice_client_gtk_2_0_la_LIBADD = $(SPICE_GTK_LIBADD_COMMON)
libspice_client_gtk_2_0_la_SOURCES = $(SPICE_GTK_SOURCES_COMMON)
nodist_libspice_client_gtk_2_0_la_SOURCES = $(nodist_SPICE_GTK_SOURCES_COMMON)
else
libspice_client_gtk_3_0_la_LDFLAGS = $(SPICE_GTK_LDFLAGS_COMMON)
libspice_client_gtk_3_0_la_LIBADD = $(SPICE_GTK_LIBADD_COMMON)
libspice_client_gtk_3_0_la_SOURCES = $(SPICE_GTK_SOURCES_COMMON)
nodist_libspice_client_gtk_3_0_la_SOURCES = $(nodist_SPICE_GTK_SOURCES_COMMON)
endif


libspice_client_gtkincludedir = $(includedir)/spice-client-gtk-$(SPICE_GTK_API_VERSION)
libspice_client_gtkinclude_HEADERS =	\
	spice-gtk-session.h		\
	spice-widget.h			\
	spice-grabsequence.h		\
	usb-device-widget.h		\
	$(NULL)

nodist_libspice_client_gtkinclude_HEADERS =	\
	spice-widget-enums.h			\
	$(NULL)

libspice_client_glib_2_0_la_LDFLAGS =	\
	-version-info 13:0:5		\
	-no-undefined			\
	$(GLIB_VERSION_LDFLAGS)		\
	$(PIE_LDFLAGS)				\
	$(NULL)

libspice_client_glib_2_0_la_CPPFLAGS = \
	$(PIE_CFLAGS) \
	$(NULL)

libspice_client_glib_2_0_la_LIBADD =					\
	$(top_builddir)/spice-common/common/libspice-common.la		\
	$(top_builddir)/spice-common/common/libspice-common-client.la	\
	$(GLIB2_LIBS)							\
	$(GIO_LIBS)							\
	$(GOBJECT2_LIBS)						\
	$(CELT051_LIBS)							\
	$(JPEG_LIBS)							\
	$(Z_LIBS)							\
	$(PIXMAN_LIBS)							\
	$(SSL_LIBS)							\
	$(PULSE_LIBS)							\
	$(GST_LIBS)							\
	$(SASL_LIBS)							\
	$(SMARTCARD_LIBS)						\
	$(USBREDIR_LIBS)						\
	$(GUDEV_LIBS)							\
	$(NULL)

if WITH_POLKIT
USB_ACL_HELPER_SRCS =				\
	usb-acl-helper.c			\
	usb-acl-helper.h			\
	$(NULL)
AM_CPPFLAGS += -DACL_HELPER_PATH="\"$(ACL_HELPER_DIR)\""
else
USB_ACL_HELPER_SRCS =
endif

libspice_client_glib_2_0_la_SOURCES =			\
	bio-gsocket.c					\
	bio-gsocket.h					\
	glib-compat.c					\
	glib-compat.h					\
	spice-audio.c					\
	spice-audio-priv.h				\
	spice-common.h					\
	spice-util.c					\
	spice-util-priv.h				\
	spice-option.h					\
	spice-option.c					\
							\
	spice-client.c					\
	spice-session.c					\
	spice-session-priv.h				\
	spice-channel.c					\
	spice-channel-cache.h				\
	spice-channel-priv.h				\
	coroutine.h					\
	gio-coroutine.c					\
	gio-coroutine.h					\
							\
	channel-base.c					\
	channel-cursor.c				\
	channel-display.c				\
	channel-display-priv.h				\
	channel-display-mjpeg.c				\
	channel-inputs.c				\
	channel-main.c					\
	channel-playback.c				\
	channel-playback-priv.h				\
	channel-port.c					\
	channel-record.c				\
	channel-smartcard.c				\
	channel-usbredir.c				\
	channel-usbredir-priv.h				\
	smartcard-manager.c				\
	smartcard-manager-priv.h			\
	spice-proxy.c					\
	spice-proxy.h					\
	usb-device-manager.c				\
	usb-device-manager-priv.h			\
	usbutil.c					\
	usbutil.h					\
	$(USB_ACL_HELPER_SRCS)				\
							\
	decode.h					\
	decode-glz.c					\
	decode-jpeg.c					\
	decode-zlib.c					\
							\
	$(top_srcdir)/spice-common/common/sw_canvas.c	\
	$(top_srcdir)/spice-common/common/sw_canvas.h	\
	$(NULL)

if WITH_GPROXY
libspice_client_glib_2_0_la_SOURCES +=			\
	wocky-http-proxy.c				\
	wocky-http-proxy.h				\
	$(NULL)
endif

nodist_libspice_client_glib_2_0_la_SOURCES =	\
	spice-glib-enums.c			\
	spice-marshal.c				\
	spice-marshal.h				\
	$(NULL)

libspice_client_glibincludedir = $(includedir)/spice-client-glib-2.0
libspice_client_glibinclude_HEADERS =	\
	spice-audio.h			\
	spice-client.h			\
	spice-types.h			\
	spice-session.h			\
	spice-channel.h			\
	spice-util.h			\
	spice-option.h			\
	channel-cursor.h		\
	channel-display.h		\
	channel-inputs.h		\
	channel-main.h			\
	channel-playback.h		\
	channel-port.h			\
	channel-record.h		\
	channel-smartcard.h		\
	channel-usbredir.h		\
	usb-device-manager.h		\
	smartcard-manager.h		\
	$(NULL)

nodist_libspice_client_glibinclude_HEADERS =	\
	spice-glib-enums.h			\
	$(NULL)

# file for API compatibility, but we don't want warning during our compilation
dist_libspice_client_glibinclude_DATA =	\
	spice-channel-enums.h		\
	$(NULL)

if WITH_PULSE
libspice_client_glib_2_0_la_SOURCES +=	\
	spice-pulse.c			\
	spice-pulse.h			\
	$(NULL)
endif

if WITH_GSTAUDIO
libspice_client_glib_2_0_la_SOURCES +=	\
	spice-gstaudio.c		\
	spice-gstaudio.h		\
	$(NULL)
endif

if WITH_UCONTEXT
libspice_client_glib_2_0_la_SOURCES += continuation.h continuation.c coroutine_ucontext.c
endif

if WITH_WINFIBER
libspice_client_glib_2_0_la_SOURCES += coroutine_winfibers.c
endif

if WITH_GTHREAD
libspice_client_glib_2_0_la_SOURCES += coroutine_gthread.c
libspice_client_glib_2_0_la_LIBADD += $(GTHREAD_LIBS)
endif


WIN_USB_FILES= \
	win-usb-dev.h			\
	win-usb-dev.c			\
	win-usb-clerk.h			\
	win-usb-driver-install.h	\
	win-usb-driver-install.c	\
	$(NULL)

if OS_WIN32
if WITH_USBREDIR
libspice_client_glib_2_0_la_SOURCES += \
	$(WIN_USB_FILES)
endif
libspice_client_glib_2_0_la_LIBADD += -lws2_32 -lgdi32
endif

spicy_SOURCES =					\
	spicy.c					\
	spice-cmdline.h				\
	spice-cmdline.c				\
	$(NULL)

spicy_LDADD =						\
	libspice-client-gtk-$(SPICE_GTK_API_VERSION).la	\
	libspice-client-glib-2.0.la			\
	$(XRANDR_LIBS)					\
	$(GTHREAD_LIBS)					\
	$(GTK_LIBS)					\
	$(LIBM)						\
	$(NULL)

spicy_CPPFLAGS =			\
	$(AM_CPPFLAGS)			\
	$(XRANDR_CFLAGS)		\
	$(GTHREAD_CFLAGS)		\
	-DSPICE_DISABLE_DEPRECATED	\
	$(NULL)


if WITH_POLKIT
spice_client_glib_usb_acl_helper_SOURCES =	\
	glib-compat.c				\
	glib-compat.h				\
	spice-client-glib-usb-acl-helper.c	\
	$(NULL)

spice_client_glib_usb_acl_helper_LDADD =	\
	$(GLIB2_LIBS)				\
	$(GIO_LIBS)				\
	$(POLKIT_LIBS)				\
	$(ACL_LIBS)				\
	$(PIE_LDFLAGS)				\
	$(NULL)

spice_client_glib_usb_acl_helper_CPPFLAGS =	\
	$(SPICE_CFLAGS)				\
	$(GLIB2_CFLAGS)				\
	$(GIO_CFLAGS)				\
	$(POLKIT_CFLAGS)			\
	$(PIE_CFLAGS)				\
	$(NULL)

install-data-hook:
	-chown root $(DESTDIR)$(acldir)/spice-client-glib-usb-acl-helper
	-chmod u+s  $(DESTDIR)$(acldir)/spice-client-glib-usb-acl-helper

endif


spicy_screenshot_SOURCES =			\
	spicy-screenshot.c			\
	spice-cmdline.h				\
	spice-cmdline.c				\
	$(NULL)

spicy_screenshot_LDADD =			\
	libspice-client-glib-2.0.la		\
	$(GOBJECT2_LIBS)			\
	$(NULL)

spicy_stats_SOURCES =			\
	spicy-stats.c			\
	spice-cmdline.h			\
	spice-cmdline.c			\
	$(NULL)

spicy_stats_LDADD =				\
	libspice-client-glib-2.0.la		\
	$(GOBJECT2_LIBS)			\
	$(NULL)



$(libspice_client_glib_2_0_la_SOURCES): spice-glib-enums.h spice-marshal.h

if HAVE_GTK_2
$(libspice_client_gtk_2_0_la_SOURCES): spice-glib-enums.h spice-widget-enums.h
else
$(libspice_client_gtk_3_0_la_SOURCES): spice-glib-enums.h spice-widget-enums.h
endif

spice-marshal.c: spice-marshal.h
spice-glib-enums.c: spice-glib-enums.h
spice-widget-enums.c: spice-widget-enums.h

spice-marshal.c: spice-marshal.txt
	$(AM_V_GEN)echo "#include \"spice-marshal.h\"" > $@ && \
		glib-genmarshal --body $< >> $@ || (rm -f $@ && exit 1)

spice-marshal.h: spice-marshal.txt
	$(AM_V_GEN)glib-genmarshal --header $< > $@ || (rm -f $@ && exit 1)

spice-glib-enums.c: spice-channel.h channel-inputs.h spice-session.h
	$(AM_V_GEN)glib-mkenums --fhead "#include <glib-object.h>\n" \
			--fhead "#include \"spice-glib-enums.h\"\n\n" \
			--fprod "\n#include \"spice-session.h\"\n" \
			--fprod "\n#include \"spice-channel.h\"\n" \
			--fprod "\n#include \"channel-inputs.h\"\n" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n" \
			--vtail "  static volatile gsize type_volatile = 0;\n\n" \
			--vtail "  if (g_once_init_enter(&type_volatile)) {\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n" \
			--vtail "    g_once_init_leave(&type_volatile, type);\n" \
			--vtail "  }\n\n" \
			--vtail "  return type;\n}\n\n" \
		$^ > $@

spice-glib-enums.h: spice-channel.h channel-inputs.h spice-session.h
	$(AM_V_GEN)glib-mkenums --fhead "#ifndef SPICE_GLIB_ENUMS_H\n" \
			--fhead "#define SPICE_GLIB_ENUMS_H\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* SPICE_CHANNEL_ENUMS_H */\n" \
			--eprod "#define SPICE_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$^ >  $@

spice-widget-enums.c: spice-widget.h
	$(AM_V_GEN)glib-mkenums --fhead "#include <glib-object.h>\n" \
			--fhead "#include \"spice-widget-enums.h\"\n\n" \
			--fprod "\n#include \"spice-widget.h\"\n" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n" \
			--vtail "  static volatile gsize type_volatile = 0;\n\n" \
			--vtail "  if (g_once_init_enter(&type_volatile)) {\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n" \
			--vtail "    g_once_init_leave(&type_volatile, type);\n" \
			--vtail "  }\n\n" \
			--vtail "  return type;\n}\n\n" \
		$< > $@

spice-widget-enums.h: spice-widget.h
	$(AM_V_GEN)glib-mkenums --fhead "#ifndef SPICE_WIDGET_ENUMS_H\n" \
			--fhead "#define SPICE_WIDGET_ENUMS_H\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* SPICE_WIDGET_ENUMS_H */\n" \
			--eprod "#define SPICE_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$< >  $@


vncdisplaykeymap.c: $(KEYMAPS)

$(KEYMAPS): $(KEYMAP_GEN) keymaps.csv

# Note despite being autogenerated these are not part of CLEANFILES, they
# are actually a part of EXTRA_DIST to avoid the need for perl(Text::CSV) by
# end users
vncdisplaykeymap_xorgevdev2xtkbd.c:
	$(AM_V_GEN)$(KEYMAP_GEN) $(srcdir)/keymaps.csv xorgevdev xtkbd > $@ || rm $@

vncdisplaykeymap_xorgkbd2xtkbd.c:
	$(AM_V_GEN)$(KEYMAP_GEN) $(srcdir)/keymaps.csv xorgkbd xtkbd > $@ || rm $@

vncdisplaykeymap_xorgxquartz2xtkbd.c:
	$(AM_V_GEN)$(KEYMAP_GEN) $(srcdir)/keymaps.csv xorgxquartz xtkbd > $@ || rm $@

vncdisplaykeymap_xorgxwin2xtkbd.c:
	$(AM_V_GEN)$(KEYMAP_GEN) $(srcdir)/keymaps.csv xorgxwin xtkbd > $@ || rm $@

vncdisplaykeymap_osx2xtkbd.c:
	$(AM_V_GEN)$(KEYMAP_GEN) $(srcdir)/keymaps.csv osx xtkbd > $@ || rm $@

vncdisplaykeymap_win322xtkbd.c:
	$(AM_V_GEN)$(KEYMAP_GEN) $(srcdir)/keymaps.csv win32 xtkbd > $@ || rm $@

vncdisplaykeymap_x112xtkbd.c:
	$(AM_V_GEN)$(KEYMAP_GEN) $(srcdir)/keymaps.csv x11 xtkbd > $@ || rm $@

if WITH_PYTHON
pyexec_LTLIBRARIES = SpiceClientGtk.la

# workaround for broken parallel install support in automake with LTLIBRARIES
# http://debbugs.gnu.org/cgi/bugreport.cgi?bug=7328
install_pyexecLTLIBRARIES = install-pyexecLTLIBRARIES
$(install_pyexecLTLIBRARIES): install-libLTLIBRARIES

SpiceClientGtk_la_LIBADD = libspice-client-gtk-2.0.la libspice-client-glib-2.0.la $(PYGTK_LIBS)
SpiceClientGtk_la_CFLAGS = $(GTK_CFLAGS) $(PYTHON_INCLUDES) $(PYGTK_CFLAGS) $(WARN_PYFLAGS)
SpiceClientGtk_la_LDFLAGS = -module -avoid-version -fPIC
SpiceClientGtk_la_SOURCES = spice-client-gtk-module.c
nodist_SpiceClientGtk_la_SOURCES = spice-client-gtk-module.defs.c

CODEGENDIR = `pkg-config --variable=codegendir pygtk-2.0`
DEFSDIR = `pkg-config --variable=defsdir pygtk-2.0`

spice-client-gtk.defs: $(libspice_client_gtkinclude_HEADERS) $(nodist_libspice_client_gtkinclude_HEADERS) $(libspice_client_glibinclude_HEADERS) $(nodist_libspice_client_glibinclude_HEADERS)
	$(AM_V_GEN)$(PYTHON) $(CODEGENDIR)/h2def.py \
		-f $(srcdir)/spice-client-gtk-manual.defs \
		$^ > $@

spice-client-gtk-module.defs.c: spice-client-gtk.override spice-client-gtk.defs spice-client-gtk-manual.defs
	@cat spice-client-gtk.defs $(srcdir)/spice-client-gtk-manual.defs > tmp.defs
	$(AM_V_GEN)pygobject-codegen-2.0 --prefix spice \
			  --register $(DEFSDIR)/gdk-types.defs \
			  --register $(DEFSDIR)/gtk-types.defs \
			  --override $(srcdir)/spice-client-gtk.override \
			  tmp.defs > $@
	@rm tmp.defs

CLEANFILES += spice-client-gtk-module.defs.c spice-client-gtk.defs
endif

-include $(INTROSPECTION_MAKEFILE)

if G_IR_SCANNER_SYMBOL_PREFIX
PREFIX_ARGS = --symbol-prefix=spice --identifier-prefix=Spice
else
PREFIX_ARGS = --strip-prefix=Spice
endif

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all --accept-unprefixed --add-include-path=$(builddir) $(PREFIX_ARGS)
INTROSPECTION_COMPILER_ARGS = --includedir=$(builddir)

if HAVE_INTROSPECTION
glib_introspection_files =				\
	$(libspice_client_glibinclude_HEADERS)		\
	$(nodist_libspice_client_glibinclude_HEADERS)	\
	spice-audio.c					\
	spice-client.c					\
	spice-session.c					\
	spice-channel.c					\
	spice-glib-enums.c				\
	spice-option.c					\
	spice-util.c					\
	channel-cursor.c				\
	channel-display.c				\
	channel-inputs.c				\
	channel-main.c					\
	channel-playback.c				\
	channel-port.c					\
	channel-record.c				\
	channel-smartcard.c				\
	channel-usbredir.c				\
	smartcard-manager.c				\
	usb-device-manager.c				\
	$(NULL)

gtk_introspection_files =				\
	$(libspice_client_gtkinclude_HEADERS)		\
	$(nodist_libspice_client_gtkinclude_HEADERS)	\
	spice-gtk-session.c				\
	spice-widget.c					\
	spice-grabsequence.c				\
	usb-device-widget.c				\
	$(NULL)

SpiceClientGLib-2.0.gir: libspice-client-glib-2.0.la
SpiceClientGLib_2_0_gir_INCLUDES = GObject-2.0 Gio-2.0
SpiceClientGLib_2_0_gir_CFLAGS = $(SPICE_COMMON_CPPFLAGS)
SpiceClientGLib_2_0_gir_LIBS = libspice-client-glib-2.0.la
SpiceClientGLib_2_0_gir_FILES = $(glib_introspection_files)
SpiceClientGLib_2_0_gir_EXPORT_PACKAGES = spice-client-glib-2.0
SpiceClientGLib_2_0_gir_SCANNERFLAGS = --c-include="spice-client.h"
INTROSPECTION_GIRS += SpiceClientGLib-2.0.gir

if HAVE_GTK_2
SpiceClientGtk-2.0.gir: libspice-client-gtk-2.0.la SpiceClientGLib-2.0.gir
SpiceClientGtk_2_0_gir_INCLUDES = GObject-2.0 Gtk-2.0 SpiceClientGLib-2.0
SpiceClientGtk_2_0_gir_CFLAGS = $(SPICE_COMMON_CPPFLAGS)
SpiceClientGtk_2_0_gir_LIBS = libspice-client-gtk-2.0.la libspice-client-glib-2.0.la
SpiceClientGtk_2_0_gir_FILES = $(gtk_introspection_files)
SpiceClientGtk_2_0_gir_EXPORT_PACKAGES = spice-client-gtk-2.0
SpiceClientGtk_2_0_gir_SCANNERFLAGS = --c-include="spice-widget.h"
else
SpiceClientGtk-3.0.gir: libspice-client-gtk-3.0.la SpiceClientGLib-2.0.gir
SpiceClientGtk_3_0_gir_INCLUDES = GObject-2.0 Gtk-3.0 SpiceClientGLib-2.0
SpiceClientGtk_3_0_gir_CFLAGS = $(SPICE_COMMON_CPPFLAGS)
SpiceClientGtk_3_0_gir_LIBS = libspice-client-gtk-3.0.la libspice-client-glib-2.0.la
SpiceClientGtk_3_0_gir_FILES = $(gtk_introspection_files)
SpiceClientGtk_3_0_gir_EXPORT_PACKAGES = spice-client-gtk-3.0
SpiceClientGtk_3_0_gir_SCANNERFLAGS = --c-include="spice-widget.h"
endif
INTROSPECTION_GIRS += SpiceClientGtk-$(SPICE_GTK_API_VERSION).gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)
endif

update-symbol-files:
	( echo "SPICEGTK_1 {" ; \
	  echo "global:" ; \
	  ctags -f - --c-kinds=p $(libspice_client_gtkinclude_HEADERS) $(nodist_libspice_client_gtkinclude_HEADERS) $(libspice_client_glibinclude_HEADERS) $(nodist_libspice_client_glibinclude_HEADERS) | awk '/^spice_/ { print $$1 ";" }' | sort ; \
	  echo "local:" ;  \
	  echo "*;" ; \
	  echo "};" ) > $(srcdir)/map-file
	( ctags -f - --c-kinds=p $(libspice_client_glibinclude_HEADERS) $(nodist_libspice_client_glibinclude_HEADERS) | awk '/^spice_/ { print $$1 }' | sort ; \
	) > $(srcdir)/spice-glib-sym-file
	( ctags -f - --c-kinds=p $(libspice_client_gtkinclude_HEADERS) $(nodist_libspice_client_gtkinclude_HEADERS) | awk '/^spice_/ { print $$1 }' | sort ; \
	) > $(srcdir)/spice-gtk-sym-file

-include $(top_srcdir)/git.mk
