NULL =

spice_protocol_includedir = $(includedir)/spice-1/spice

spice_protocol_include_HEADERS =		\
	barrier.h				\
	controller_prot.h			\
	end-packed.h				\
	enums.h					\
	error_codes.h				\
	foreign_menu_prot.h			\
	ipc_ring.h				\
	macros.h				\
	protocol.h				\
	qxl_dev.h				\
	qxl_windows.h				\
	start-packed.h				\
	stats.h					\
	types.h					\
	vd_agent.h				\
	vdi_dev.h				\
	$(NULL)

# Using the python code generator requires some python modules
# which might not be installed on the user computer. Given that
# enums.h is stored in git, and should be up-to-date anyway,
# we can make this part optional
if ENABLE_CODEGEN
MARSHALLERS_DEPS =					\
	$(top_srcdir)/python_modules/__init__.py	\
	$(top_srcdir)/python_modules/codegen.py		\
	$(top_srcdir)/python_modules/demarshal.py	\
	$(top_srcdir)/python_modules/marshal.py		\
	$(top_srcdir)/python_modules/ptypes.py		\
	$(top_srcdir)/python_modules/spice_parser.py	\
	$(top_srcdir)/spice_codegen.py			\
	$(NULL)

BUILT_SOURCES = enums.h

# this is going to upset automake distcheck, since we try to write to
# readonly srcdir. To limit the fail chances, rebuild automatically
# enums.h only if the spice.proto has changed.
enums.h: $(top_srcdir)/spice.proto # $(MARSHALLERS_DEPS)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/spice_codegen.py --generate-enums $< $@ >/dev/null
endif

-include $(top_srcdir)/git.mk
