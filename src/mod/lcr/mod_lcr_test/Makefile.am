include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_lcr_test

mod_LTLIBRARIES = mod_lcr_test.la
mod_lcr_test_la_SOURCES  = mod_lcr_test.c
mod_lcr_test_la_CFLAGS   = $(shell pkg-config --cflags libwebsockets) $(AM_CFLAGS)
mod_lcr_test_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_lcr_test_la_LDFLAGS  = $(shell pkg-config --libs libwebsockets) -avoid-version -module -no-undefined -shared

$(info OPENLDAP_LIBS_TXT: $(OPENLDAP_LIBS))