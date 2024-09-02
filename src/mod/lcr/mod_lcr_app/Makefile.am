include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_lcr_app

mod_LTLIBRARIES = mod_lcr_app.la
mod_lcr_app_la_SOURCES  = mod_lcr_app.c
mod_lcr_app_la_CFLAGS   = $(AM_CFLAGS)
mod_lcr_app_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_lcr_app_la_LDFLAGS  = -avoid-version -module -no-undefined -shared