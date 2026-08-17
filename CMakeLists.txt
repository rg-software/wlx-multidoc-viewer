cmake_minimum_required(VERSION 3.20)
project(wlx-multidoc-viewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MUPDF REQUIRED IMPORTED_TARGET mupdf)
pkg_check_modules(DJVU REQUIRED IMPORTED_TARGET ddjvu)

add_library(wlx-multidoc-viewer SHARED
    src/plugin.cpp
    src/formatdispatcher.cpp
    src/mupdfengine.cpp
    src/djvuengine.cpp
    src/viewer.cpp
)

target_include_directories(wlx-multidoc-viewer PRIVATE src)
target_link_libraries(wlx-multidoc-viewer
    Qt6::Widgets
    PkgConfig::MUPDF
    PkgConfig::DJVU
)

set_target_properties(wlx-multidoc-viewer PROPERTIES
    PREFIX ""
    OUTPUT_NAME "wlx-multidoc-viewer"
)
