include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_tencent_asr
$(info TENCNET_AM_CFLAGS: $(AM_CFLAGS))
$(info TENCNET_AM_CPPFLAGS: $(AM_CPPFLAGS))
mod_LTLIBRARIES = mod_tencent_asr.la
mod_tencent_asr_la_SOURCES  = mod_tencent_asr.cpp
mod_tencent_asr_la_CFLAGS   = $(AM_CFLAGS)
mod_tencent_asr_la_CXXFLAGS  = -std=c++11
mod_tencent_asr_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/mod/lcr/mod_tencent_asr/include
mod_tencent_asr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la -lpthread -ldl -lboost_system -lboost_thread -lssl -lcrypto
mod_tencent_asr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -D_GLIBCXX_USE_CXX11_ABI=0

mod_tencent_asr_la_SOURCES  += ./src/speech_recognizer.cpp ./src/tcloud_public_request_build.cpp ./src/tcloud_util.cpp