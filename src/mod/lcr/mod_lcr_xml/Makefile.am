include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_lcr_xml

mod_LTLIBRARIES = mod_lcr_xml.la
mod_lcr_xml_la_SOURCES  = mod_lcr_xml.c
mod_lcr_xml_la_CFLAGS   = $(AM_CFLAGS)
mod_lcr_xml_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_lcr_xml_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

#输出内容
$(info OPENLDAP_LIBS_TXT: $(OPENLDAP_LIBS))