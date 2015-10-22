NULL=

# TODO: not sure if we want to do that
#desktopdir       = $(datadir)/applications
#desktop_in_files = spicy.desktop.in
#desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)
#@INTLTOOL_DESKTOP_RULE@
#
#MIMEFILES = spice-mime.xml.in
#mimedir = $(datadir)/mime/packages
#mime_DATA = spice-mime.xml
#@INTLTOOL_XML_RULE@

EXTRA_DIST =					\
	spicy.nsis				\
	spice-mime.xml.in spicy.desktop.in      \
	spice-protocol.vapi			\
	gtkrc					\
	$(desktop_in_files)			\
	org.spice-space.lowlevelusbaccess.policy \
	$(NULL)

CLEANFILES =					\
	$(desktop_DATA)				\
	$(NULL)

DISTCLEANFILES =				\
	$(desktop_DATA)				\
	$(NULL)

vapidir = $(VAPIDIR)
vapi_DATA = spice-protocol.vapi

policydir = $(POLICYDIR)
policy_DATA = org.spice-space.lowlevelusbaccess.policy

-include $(top_srcdir)/git.mk
