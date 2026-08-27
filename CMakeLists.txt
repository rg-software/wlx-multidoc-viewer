cmake_minimum_required(VERSION 3.20)
project(wlx-multidoc-viewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- MuPDF -----------------------------------------------------------
if(WIN32)
    find_package(unofficial-libmupdf CONFIG REQUIRED)
    set(MUPDF_TARGET unofficial::libmupdf::libmupdf)
else()
    find_library(MUPDF_LIBRARY NAMES mupdf REQUIRED)
    find_path(MUPDF_INCLUDE_DIR NAMES mupdf/fitz.h REQUIRED)
    add_library(unofficial::libmupdf::libmupdf UNKNOWN IMPORTED)
    set_target_properties(unofficial::libmupdf::libmupdf PROPERTIES
        IMPORTED_LOCATION "${MUPDF_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MUPDF_INCLUDE_DIR}"
    )
    set(MUPDF_TARGET unofficial::libmupdf::libmupdf)
endif()

# --- DjVuLibre ------------------------------------------------------
if(WIN32)
    find_package(djvulibre CONFIG REQUIRED)
    set(DJVU_TARGET djvulibre::djvulibre)
else()
    find_library(DJVULIBRE_LIBRARY NAMES djvulibre REQUIRED)
    find_path(DJVULIBRE_INCLUDE_DIR NAMES libdjvu/ddjvuapi.h REQUIRED)
    add_library(djvulibre::djvulibre UNKNOWN IMPORTED)
    set_target_properties(djvulibre::djvulibre PROPERTIES
        IMPORTED_LOCATION "${DJVULIBRE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${DJVULIBRE_INCLUDE_DIR}"
    )
    set(DJVU_TARGET djvulibre::djvulibre)
endif()

# --- CHMLib ---------------------------------------------------------
# The upstream vcpkg port and system libchm packages ship only the header
# plus the static library (no CMake package config), so locate it manually
# and wrap it in an imported target like the fallbacks above.
find_path(CHMLIB_INCLUDE_DIR NAMES chm_lib.h REQUIRED)
find_library(CHMLIB_LIBRARY NAMES chm REQUIRED)
add_library(chmlib::chmlib UNKNOWN IMPORTED)
set_target_properties(chmlib::chmlib PROPERTIES
    IMPORTED_LOCATION "${CHMLIB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CHMLIB_INCLUDE_DIR}"
)

# --- LibArchive -----------------------------------------------------
# Reads RAR4/RAR5/7z/ZIP/TAR comic containers. Like CHMLib, neither the
# vcpkg port nor system packages ship a clean imported-target config, so
# locate the pair manually on every platform. The static archive drags its
# codec backends (lz4/xz/zstd/bzip2/openssl) plus Windows system libs along
# at link time; resolve per-config variants (vcpkg suffixes some debug libs
# with a 'd') and attach them as transitive dependencies.
find_path(LIBARCHIVE_INCLUDE_DIR NAMES archive.h REQUIRED)

if(WIN32 AND VCPKG_TOOLCHAIN)
    # Manifest mode installs per-config trees under ${VCPKG_INSTALLED_DIR}.
    set(_LA_TRIPLET_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    find_library(LIBARCHIVE_LIBRARY_RELEASE NAMES archive
        HINTS "${_LA_TRIPLET_DIR}/lib" NO_DEFAULT_PATH)
    find_library(LIBARCHIVE_LIBRARY_DEBUG NAMES archived archive
        HINTS "${_LA_TRIPLET_DIR}/debug/lib" NO_DEFAULT_PATH)
    unset(_LA_TRIPLET_DIR)
    if(NOT LIBARCHIVE_LIBRARY_RELEASE)
        message(FATAL_ERROR "LibArchive static library not found in vcpkg tree")
    endif()
else()
    find_library(LIBARCHIVE_LIBRARY_RELEASE NAMES archive REQUIRED)
    set(LIBARCHIVE_LIBRARY_DEBUG "")
endif()

add_library(LibArchive::LibArchive UNKNOWN IMPORTED)
if(LIBARCHIVE_LIBRARY_RELEASE)
    set_target_properties(LibArchive::LibArchive PROPERTIES
        IMPORTED_LOCATION_RELEASE "${LIBARCHIVE_LIBRARY_RELEASE}"
        MAP_IMPORTED_CONFIG_MINSIZE "Release"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO "Release")
endif()
if(LIBARCHIVE_LIBRARY_DEBUG)
    set_target_properties(LibArchive::LibArchive PROPERTIES
        IMPORTED_LOCATION_DEBUG "${LIBARCHIVE_LIBRARY_DEBUG}")
endif()

get_filename_component(_LA_REL_DIR "${LIBARCHIVE_LIBRARY_RELEASE}" DIRECTORY)
set(_LA_DBG_DIR "${_LA_REL_DIR}/debug")
function(la_link_codec _dep)
    find_library(${_dep}_REL NAMES ${_dep}   HINTS "${_LA_REL_DIR}" NO_DEFAULT_PATH)
    find_library(${_dep}_DBG NAMES ${_dep}d ${_dep} HINTS "${_LA_DBG_DIR}" NO_DEFAULT_PATH)
    if(${_dep}_REL AND ${_dep}_DBG)
        set_property(TARGET LibArchive::LibArchive APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES
            "$<IF:$<CONFIG:Debug>,${${_dep}_DBG},${${_dep}_REL}>")
    elseif(${_dep}_REL)
        set_property(TARGET LibArchive::LibArchive APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES "${${_dep}_REL}")
    endif()
    unset(${_dep}_REL CACHE)
    unset(${_dep}_DBG CACHE)
endfunction()

foreach(_dep lz4 lzma zstd bz2 libssl libcrypto)
    la_link_codec(${_dep})
endforeach()
unset(_LA_REL_DIR CACHE)
unset(_LA_DBG_DIR CACHE)
if(WIN32)
    # xmllite: XAR TOC reader; crypt32/ws2_32/bcrypt: static OpenSSL backends.
    target_link_libraries(LibArchive::LibArchive INTERFACE
        xmllite crypt32 ws2_32 bcrypt)
endif()

# --- Platform-specific Qt -------------------------------------------
if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui)
else()
    find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
    set(CMAKE_AUTOMOC ON)
endif()

# --- Embed the toolbar icon font (Material Symbols Outlined). -----------------
# A variable TTF is turned into a byte-array header at configure time so the
# plugin needs no runtime asset file on either platform.
set(MS_FONT_SRC "${CMAKE_SOURCE_DIR}/assets/MaterialSymbolsOutlined.ttf")
set(MS_FONT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/material_symbols_fontdata.h")
add_custom_command(
    OUTPUT "${MS_FONT_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DIN=${MS_FONT_SRC} -DOUT=${MS_FONT_HEADER} -DVARNAME=MaterialSymbolsTtf
        -P ${CMAKE_SOURCE_DIR}/cmake/embed_font.cmake
    DEPENDS "${MS_FONT_SRC}"
    VERBATIM
)

# --- Library ---------------------------------------------------------
add_library(wlx-multidoc-viewer SHARED
    src/plugin.cpp
    src/formatdispatcher.cpp
    src/mupdfengine.cpp
    src/djvuengine.cpp
    src/chmengine.cpp
        src/comicengine.cpp
    src/viewercontroller.cpp
    src/textselection.cpp
    src/searchcontroller.cpp
    src/toolbar.cpp
    src/toolbar_icons.cpp
    src/printcoordinator.cpp
)

if(NOT WIN32)
    target_sources(wlx-multidoc-viewer PRIVATE
        src/viewer.cpp
        src/toolbar_qt.cpp
        src/sidebar_qt.cpp
        src/print_qt.cpp
    )
else()
    target_sources(wlx-multidoc-viewer PRIVATE
        src/viewer_win32.cpp
        src/toolbar_win32.cpp
        src/sidebar_win32.cpp
        src/print_win32.cpp
        src/version.rc
    )
endif()

add_custom_target(material_symbols_font_header DEPENDS "${MS_FONT_HEADER}")

target_include_directories(wlx-multidoc-viewer PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(wlx-multidoc-viewer material_symbols_font_header)

if(WIN32)
    target_link_libraries(wlx-multidoc-viewer
        Qt6::Core
        Qt6::Gui
        ${MUPDF_TARGET}
        ${DJVU_TARGET}
        LibArchive::LibArchive
        chmlib::chmlib
    )
else()
    target_link_libraries(wlx-multidoc-viewer
        Qt6::Widgets
        Qt6::PrintSupport
        ${MUPDF_TARGET}
        ${DJVU_TARGET}
        LibArchive::LibArchive
        chmlib::chmlib
    )
endif()

set_target_properties(wlx-multidoc-viewer PROPERTIES
    PREFIX ""
    OUTPUT_NAME "MultidocViewer"
)

# Lister plugin destination for Double Commander: <prefix>/share/doublecmd/plugins/multidoc
include(GNUInstallDirs)
install(TARGETS wlx-multidoc-viewer
        LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/doublecmd/plugins/multidoc)

if(WIN32)
    # Total Commander loads the 64-bit build as .wlx64 and the 32-bit build as
    # .wlx, so pick the suffix from pointer size, not just the platform.
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".wlx64")
    else()
        set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".wlx")
    endif()
    target_compile_definitions(wlx-multidoc-viewer PRIVATE WLX_MULTIDOC_VIEWER_EXPORTS)
elseif(APPLE)
    set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".wlx")
else()
    set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".wlx64")
endif()

option(WLX_BUILD_HARNESS "Build the Win32 drag-test harness executable" OFF)
if(WLX_BUILD_HARNESS AND WIN32)
    add_executable(harness-win
        tests/harness_win.cpp
        src/viewer_win32.cpp
        src/toolbar_win32.cpp
        src/sidebar_win32.cpp
        src/viewercontroller.cpp
        src/textselection.cpp
        src/formatdispatcher.cpp
        src/mupdfengine.cpp
        src/djvuengine.cpp
        src/chmengine.cpp
        src/comicengine.cpp
        src/searchcontroller.cpp
        src/toolbar.cpp
        src/toolbar_icons.cpp
        src/printcoordinator.cpp
        src/print_win32.cpp
    )
    target_include_directories(harness-win PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}")
    add_dependencies(harness-win material_symbols_font_header)
    target_link_libraries(harness-win
        Qt6::Core
        Qt6::Gui
        ${MUPDF_TARGET}
        ${DJVU_TARGET}
        LibArchive::LibArchive
        chmlib::chmlib
    )

    add_executable(harness-scroll
        tests/harness_scroll.cpp
        src/viewer_win32.cpp
        src/toolbar_win32.cpp
        src/sidebar_win32.cpp
        src/viewercontroller.cpp
        src/textselection.cpp
        src/formatdispatcher.cpp
        src/mupdfengine.cpp
        src/djvuengine.cpp
        src/chmengine.cpp
        src/comicengine.cpp
        src/searchcontroller.cpp
        src/toolbar.cpp
        src/toolbar_icons.cpp
        src/printcoordinator.cpp
        src/print_win32.cpp
    )
    target_include_directories(harness-scroll PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}")
    add_dependencies(harness-scroll material_symbols_font_header)
    target_link_libraries(harness-scroll
        Qt6::Core
        Qt6::Gui
        ${MUPDF_TARGET}
        ${DJVU_TARGET}
        LibArchive::LibArchive
        chmlib::chmlib
    )
endif()
