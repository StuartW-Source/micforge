# MicForge — CMake build (the planned replacement for micforge.pro; both are kept in sync during the move).
#
#   cmake -B build-cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-cmake
#   ctest --test-dir build-cmake --output-on-failure
#   cmake --install build-cmake --prefix ~/.local
cmake_minimum_required(VERSION 3.21)
project(MicForge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------- options
if(LINUX)
    set(_default_backend pipewire)
else()
    set(_default_backend none)
endif()
set(MICFORGE_BACKEND ${_default_backend} CACHE STRING "Audio backend: pipewire (Linux). wasapi is planned for Windows.")
option(MICFORGE_WITH_LV2  "Host LV2 plugins (lilv is loaded at run time)" ${LINUX})
option(MICFORGE_WITH_VST3 "Host VST3 plugins (Steinberg VST3 SDK, MIT)" ON)
option(MICFORGE_BUILD_TESTS "Build the automated tests" ON)

if(NOT MICFORGE_BACKEND STREQUAL "pipewire")
    message(FATAL_ERROR "MICFORGE_BACKEND=${MICFORGE_BACKEND}: only the PipeWire backend exists so far (see docs/CROSS_PLATFORM.md).")
endif()
if(NOT MICFORGE_WITH_VST3)
    message(FATAL_ERROR "Building without VST3 isn't supported yet.")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Network Concurrent DBus)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
if(MICFORGE_WITH_LV2)
    pkg_check_modules(LILV REQUIRED lilv-0)   # headers only; the library is loaded at run time
endif()

# ---------------------------------------------------------------- VST3 SDK (only the hosting pieces)
set(VST3SDK ${CMAKE_SOURCE_DIR}/third_party/vst3sdk)
if(NOT EXISTS ${VST3SDK}/pluginterfaces/base/funknown.h)
    message(FATAL_ERROR "VST3 SDK missing: run ./scripts/fetch-vst3sdk.sh (or git submodule update --init)")
endif()
add_library(vst3host STATIC
    ${VST3SDK}/pluginterfaces/base/conststringtable.cpp
    ${VST3SDK}/pluginterfaces/base/coreiids.cpp
    ${VST3SDK}/pluginterfaces/base/funknown.cpp
    ${VST3SDK}/pluginterfaces/base/ustring.cpp
    ${VST3SDK}/base/source/baseiids.cpp
    ${VST3SDK}/base/source/fbuffer.cpp
    ${VST3SDK}/base/source/fdebug.cpp
    ${VST3SDK}/base/source/fobject.cpp
    ${VST3SDK}/base/source/fstreamer.cpp
    ${VST3SDK}/base/source/fstring.cpp
    ${VST3SDK}/base/source/updatehandler.cpp
    ${VST3SDK}/base/thread/source/flock.cpp
    ${VST3SDK}/base/thread/source/fcondition.cpp
    ${VST3SDK}/public.sdk/source/common/commoniids.cpp
    ${VST3SDK}/public.sdk/source/common/commonstringconvert.cpp
    ${VST3SDK}/public.sdk/source/common/memorystream.cpp
    ${VST3SDK}/public.sdk/source/vst/vstinitiids.cpp
    ${VST3SDK}/public.sdk/source/vst/utility/stringconvert.cpp
    ${VST3SDK}/public.sdk/source/vst/hosting/module.cpp
    ${VST3SDK}/public.sdk/source/vst/hosting/plugprovider.cpp
    ${VST3SDK}/public.sdk/source/vst/hosting/hostclasses.cpp
    ${VST3SDK}/public.sdk/source/vst/hosting/parameterchanges.cpp
    ${VST3SDK}/public.sdk/source/vst/hosting/processdata.cpp
    ${VST3SDK}/public.sdk/source/vst/hosting/connectionproxy.cpp
    ${VST3SDK}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
    ${VST3SDK}/public.sdk/source/vst/hosting/eventlist.cpp)
if(LINUX)
    target_sources(vst3host PRIVATE
        ${VST3SDK}/public.sdk/source/common/threadchecker_linux.cpp
        ${VST3SDK}/public.sdk/source/vst/hosting/module_linux.cpp)
elseif(WIN32)
    target_sources(vst3host PRIVATE
        ${VST3SDK}/public.sdk/source/common/threadchecker_win32.cpp
        ${VST3SDK}/public.sdk/source/vst/hosting/module_win32.cpp)
endif()
target_include_directories(vst3host PUBLIC ${VST3SDK})
target_compile_definitions(vst3host PUBLIC RELEASE=1)
set_target_properties(vst3host PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(vst3host PRIVATE -w)   # third-party code
endif()
target_link_libraries(vst3host PUBLIC ${CMAKE_DL_LIBS})

# ---------------------------------------------------------------- application
add_executable(micforge
    src/main.cpp src/startup.cpp src/controller.cpp src/pluginmanager.cpp src/diagnostics.cpp src/mictest.cpp src/calibprofiles.cpp src/detectorspectrum.cpp src/presetfile.cpp
    src/audio/audiocore.cpp src/audio/pipewirebackend.cpp
    src/plugins/plugins.cpp src/plugins/vst3host.cpp
    src/optional/rnnoise_api.cpp
    src/platform/autostart_xdg.cpp src/platform/globalshortcuts_portal.cpp
    resources.qrc)
if(MICFORGE_WITH_LV2)
    target_sources(micforge PRIVATE src/plugins/lv2host.cpp src/optional/lilv_api.cpp)
    target_include_directories(micforge PRIVATE ${LILV_INCLUDE_DIRS})
else()
    target_sources(micforge PRIVATE src/plugins/lv2_disabled.cpp)
endif()
target_include_directories(micforge PRIVATE src)
target_compile_definitions(micforge PRIVATE MICFORGE_VERSION="${PROJECT_VERSION}")
target_link_libraries(micforge PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Concurrent Qt6::DBus
    PkgConfig::PIPEWIRE vst3host Threads::Threads ${CMAKE_DL_LIBS})

include(GNUInstallDirs)
install(TARGETS micforge RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(LINUX)
    install(FILES data/micforge.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/micforge.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES data/io.github.stuartw_source.micforge.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/micforge)
endif()

# ---------------------------------------------------------------- tests
if(MICFORGE_BUILD_TESTS)
    enable_testing()
    # DSP + voice-separation tests: no Qt, no audio backend (also the first thing to run on Windows).
    add_executable(dsp_test tests/dsp_test.cpp src/optional/rnnoise_api.cpp)
    target_include_directories(dsp_test PRIVATE src)
    target_link_libraries(dsp_test PRIVATE ${CMAKE_DL_LIBS})
    add_test(NAME dsp COMMAND dsp_test)

    add_executable(audiocore_test tests/audiocore_test.cpp src/audio/audiocore.cpp src/optional/rnnoise_api.cpp)
    target_include_directories(audiocore_test PRIVATE src)
    target_link_libraries(audiocore_test PRIVATE Qt6::Core Threads::Threads ${CMAKE_DL_LIBS})
    add_test(NAME audiocore COMMAND audiocore_test)

    add_executable(privacy_test tests/privacy_test.cpp)
    target_include_directories(privacy_test PRIVATE src)
    target_link_libraries(privacy_test PRIVATE Qt6::Core)
    add_test(NAME privacy COMMAND privacy_test)

    add_executable(calibprofiles_test tests/calibprofiles_test.cpp src/calibprofiles.cpp)
    target_include_directories(calibprofiles_test PRIVATE src)
    target_link_libraries(calibprofiles_test PRIVATE Qt6::Core)
    add_test(NAME calibprofiles COMMAND calibprofiles_test)

    add_executable(presetfile_test tests/presetfile_test.cpp src/presetfile.cpp)
    target_include_directories(presetfile_test PRIVATE src)
    target_link_libraries(presetfile_test PRIVATE Qt6::Core)
    add_test(NAME presetfile COMMAND presetfile_test)

    add_executable(undostack_test tests/undostack_test.cpp)
    target_include_directories(undostack_test PRIVATE src)
    target_link_libraries(undostack_test PRIVATE Qt6::Core)
    add_test(NAME undostack COMMAND undostack_test)

    # Controller (undo, presets, import/export) with private settings folders.
    get_target_property(_mf_sources micforge SOURCES)
    list(FILTER _mf_sources EXCLUDE REGEX "src/main\\.cpp$")
    add_executable(controller_test tests/controller_test.cpp ${_mf_sources})
    target_include_directories(controller_test PRIVATE src ${LILV_INCLUDE_DIRS})
    target_compile_definitions(controller_test PRIVATE MICFORGE_VERSION="${PROJECT_VERSION}")
    target_link_libraries(controller_test PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Concurrent Qt6::DBus
        PkgConfig::PIPEWIRE vst3host Threads::Threads ${CMAKE_DL_LIBS})
    add_test(NAME controller COMMAND controller_test)

    add_executable(autostart_test tests/autostart_test.cpp src/platform/autostart_xdg.cpp)
    target_include_directories(autostart_test PRIVATE src)
    target_link_libraries(autostart_test PRIVATE Qt6::Core)
    add_test(NAME autostart COMMAND autostart_test)

    if(LINUX)
        add_test(NAME startup COMMAND ${CMAKE_SOURCE_DIR}/tests/startup_test.sh $<TARGET_FILE:micforge>)
        add_test(NAME reconnect COMMAND ${CMAKE_SOURCE_DIR}/tests/reconnect_test.sh $<TARGET_FILE:micforge>)
        set_tests_properties(startup reconnect PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
    endif()
endif()
