NULL =
CLEANFILES =

vapidir = $(datadir)/vala/vapi
vapi_DATA =						\
	spice-client-glib-2.0.vapi			\
	$(NULL)
dist_vapi_DATA =					\
	spice-client-glib-2.0.deps			\
	$(NULL)

if WITH_GTK
vapi_DATA += spice-client-gtk-$(SPICE_GTK_API_VERSION).vapi
dist_vapi_DATA += spice-client-gtk-$(SPICE_GTK_API_VERSION).deps
endif

EXTRA_DIST =						\
	spice-client-gtk-2.0.deps			\
	spice-client-gtk-3.0.deps			\
	SpiceClientGLib-2.0.metadata			\
	$(NULL)

CLEANFILES += $(vapi_DATA)

spice-client-glib-2.0.vapi: $(top_builddir)/src/SpiceClientGLib-2.0.gir SpiceClientGLib-2.0.metadata
	$(AM_V_GEN)$(VAPIGEN) -q		\
		--metadatadir=$(srcdir)		\
		--library spice-client-glib-2.0	\
		--pkg gio-2.0			\
		$<

spice-client-gtk-$(SPICE_GTK_API_VERSION).vapi: $(top_builddir)/src/SpiceClientGtk-$(SPICE_GTK_API_VERSION).gir spice-client-glib-2.0.vapi
	$(AM_V_GEN)$(VAPIGEN) -q					\
		--vapidir=$(builddir)					\
		--girdir=$(top_builddir)/src				\
		--pkg spice-client-glib-2.0				\
		--pkg gtk+-$(GTK_API_VERSION)				\
		--library spice-client-gtk-$(SPICE_GTK_API_VERSION)	\
		$<

-include $(top_srcdir)/git.mk
