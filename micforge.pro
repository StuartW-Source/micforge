TEMPLATE = app
TARGET = micforge
VERSION = 0.1.0

QT += core gui widgets qml quick quickcontrols2 network concurrent
CONFIG += c++20 link_pkgconfig release
PKGCONFIG += libpipewire-0.3
# Optional features are loaded at run time (src/optional/), not linked: a missing library must
# never stop MicForge from starting. lilv's headers are still needed to build.
QMAKE_CXXFLAGS += $$system(pkg-config --cflags lilv-0)
DEFINES += MICFORGE_VERSION=\\\"$$VERSION\\\" RELEASE=1
QMAKE_CXXFLAGS_RELEASE += -O2
QMAKE_CXXFLAGS += -Wno-missing-field-initializers
LIBS += -ldl -lpthread

# Steinberg VST3 SDK (MIT), fetched into third_party/vst3sdk (see README / PKGBUILD).
VST3SDK = $$PWD/third_party/vst3sdk
!exists($$VST3SDK/pluginterfaces/base/funknown.h): error("VST3 SDK missing: run ./scripts/fetch-vst3sdk.sh")
INCLUDEPATH += $$VST3SDK src

SOURCES += \
    src/main.cpp src/startup.cpp src/diagnostics.cpp src/mictest.cpp src/calibprofiles.cpp src/controller.cpp src/audio/audiocore.cpp src/audio/pipewirebackend.cpp src/pluginmanager.cpp \
    src/plugins/plugins.cpp src/plugins/lv2host.cpp src/plugins/vst3host.cpp \
    src/optional/rnnoise_api.cpp src/optional/lilv_api.cpp src/platform/autostart_xdg.cpp

HEADERS += \
    src/params.h src/dsp.h src/effects.h src/separation.h src/audio/audiocore.h src/audio/audiobackend.h src/audio/pipewirebackend.h src/controller.h \
    src/pluginmanager.h src/plugins/plugins.h src/startup.h src/diagnostics.h src/mictest.h src/levels.h src/privacy.h src/calibprofiles.h \
    src/platform/dynlib.h src/platform/autostart.h src/optional/rnnoise_api.h src/optional/lilv_api.h

# Only the SDK pieces a host needs (no GUI toolkit, no plugin-side code).
SOURCES += \
    $$VST3SDK/pluginterfaces/base/conststringtable.cpp \
    $$VST3SDK/pluginterfaces/base/coreiids.cpp \
    $$VST3SDK/pluginterfaces/base/funknown.cpp \
    $$VST3SDK/pluginterfaces/base/ustring.cpp \
    $$VST3SDK/base/source/baseiids.cpp \
    $$VST3SDK/base/source/fbuffer.cpp \
    $$VST3SDK/base/source/fdebug.cpp \
    $$VST3SDK/base/source/fobject.cpp \
    $$VST3SDK/base/source/fstreamer.cpp \
    $$VST3SDK/base/source/fstring.cpp \
    $$VST3SDK/base/source/updatehandler.cpp \
    $$VST3SDK/base/thread/source/flock.cpp \
    $$VST3SDK/base/thread/source/fcondition.cpp \
    $$VST3SDK/public.sdk/source/common/commoniids.cpp \
    $$VST3SDK/public.sdk/source/common/commonstringconvert.cpp \
    $$VST3SDK/public.sdk/source/common/memorystream.cpp \
    $$VST3SDK/public.sdk/source/common/threadchecker_linux.cpp \
    $$VST3SDK/public.sdk/source/vst/vstinitiids.cpp \
    $$VST3SDK/public.sdk/source/vst/utility/stringconvert.cpp \
    $$VST3SDK/public.sdk/source/vst/hosting/module.cpp \
    $$VST3SDK/public.sdk/source/vst/hosting/module_linux.cpp \
    $$VST3SDK/public.sdk/source/vst/hosting/plugprovider.cpp \
    $$VST3SDK/public.sdk/source/vst/hosting/hostclasses.cpp \
    $$VST3SDK/public.sdk/source/vst/hosting/parameterchanges.cpp \
    $$VST3SDK/public.sdk/source/vst/hosting/processdata.cpp \
    $$VST3SDK/public.sdk/source/vst/hosting/connectionproxy.cpp \
    $$VST3SDK/public.sdk/source/vst/hosting/pluginterfacesupport.cpp \
    $$VST3SDK/public.sdk/source/vst/hosting/eventlist.cpp

RESOURCES += resources.qrc

# Installation (PREFIX defaults to /usr/local; packages use PREFIX=/usr).
isEmpty(PREFIX): PREFIX = /usr/local
target.path = $$PREFIX/bin
desktop.files = data/micforge.desktop
desktop.path = $$PREFIX/share/applications
icon.files = icons/micforge.svg
icon.path = $$PREFIX/share/icons/hicolor/scalable/apps
metainfo.files = data/io.github.stuartw_source.micforge.metainfo.xml
metainfo.path = $$PREFIX/share/metainfo
license.files = LICENSE
license.path = $$PREFIX/share/licenses/micforge
INSTALLS += target desktop icon metainfo license
