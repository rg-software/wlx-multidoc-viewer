cmake_minimum_required(VERSION 3.20)
project(wlx-multidoc-viewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- MuPDF -----------------------------------------------------------
find_package(unofficial-libmupdf CONFIG REQUIRED)

# --- DjVuLibre ------------------------------------------------------
find_package(djvulibre CONFIG REQUIRED)

# --- Platform-specific Qt -------------------------------------------
if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui)
else()
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    set(CMAKE_AUTOMOC ON)
endif()

# --- Library ---------------------------------------------------------
add_library(wlx-multidoc-viewer SHARED
    src/plugin.cpp
    src/formatdispatcher.cpp
    src/mupdfengine.cpp
    src/djvuengine.cpp
    src/viewercontroller.cpp
)

if(NOT WIN32)
    target_sources(wlx-multidoc-viewer PRIVATE src/viewer.cpp)
else()
    target_sources(wlx-multidoc-viewer PRIVATE src/viewer_win32.cpp)
endif()

target_include_directories(wlx-multidoc-viewer PRIVATE src)

set(MUPDF_TARGET unofficial::libmupdf::libmupdf)
set(DJVU_TARGET djvulibre::djvulibre)

if(WIN32)
    target_link_libraries(wlx-multidoc-viewer
        Qt6::Core
        Qt6::Gui
        ${MUPDF_TARGET}
        ${DJVU_TARGET}
    )
else()
    target_link_libraries(wlx-multidoc-viewer
        Qt6::Widgets
        ${MUPDF_TARGET}
        ${DJVU_TARGET}
    )
endif()

set_target_properties(wlx-multidoc-viewer PROPERTIES
    PREFIX ""
    OUTPUT_NAME "wlx-multidoc-viewer"
)

if(WIN32)
    set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".wlx64")
    target_compile_definitions(wlx-multidoc-viewer PRIVATE WLX_MULTIDOC_VIEWER_EXPORTS)
elseif(APPLE)
    set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".wlx")
else()
    set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".so")
endif()

option(WLX_BUILD_HARNESS "Build the Win32 drag-test harness executable" OFF)
if(WLX_BUILD_HARNESS AND WIN32)
    add_executable(harness-win
        tests/harness_win.cpp
        src/viewer_win32.cpp
        src/viewercontroller.cpp
        src/formatdispatcher.cpp
        src/mupdfengine.cpp
        src/djvuengine.cpp
    )
    target_include_directories(harness-win PRIVATE src)
    target_link_libraries(harness-win
        Qt6::Core
        Qt6::Gui
        ${MUPDF_TARGET}
        ${DJVU_TARGET}
    )

    add_executable(harness-scroll
        tests/harness_scroll.cpp
        src/viewer_win32.cpp
        src/viewercontroller.cpp
        src/formatdispatcher.cpp
        src/mupdfengine.cpp
        src/djvuengine.cpp
    )
    target_include_directories(harness-scroll PRIVATE src)
    target_link_libraries(harness-scroll
        Qt6::Core
        Qt6::Gui
        ${MUPDF_TARGET}
        ${DJVU_TARGET}
    )
endif()
