NULL =

dist_man_MANS =					\
	spice-client.1				\
	$(NULL)

EXTRA_DIST =					\
	spice-client.pod			\
	$(NULL)

MAINTAINERCLEANFILES = $(dist_man_MANS)

%.1: %.pod
	$(AM_V_GEN)pod2man -c "Spice-GTK Documentation" $< > $@

-include $(top_srcdir)/git.mk
