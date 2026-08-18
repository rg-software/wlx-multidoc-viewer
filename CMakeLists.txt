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
)

if(NOT WIN32)
    target_sources(wlx-multidoc-viewer PRIVATE src/viewer.cpp)
else()
    target_sources(wlx-multidoc-viewer PRIVATE src/viewer_win32.cpp)
endif()

target_include_directories(wlx-multidoc-viewer PRIVATE src)

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
    set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".dll")
    target_compile_definitions(wlx-multidoc-viewer PRIVATE WLX_MULTIDOC_VIEWER_EXPORTS)
elseif(APPLE)
    set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".wlx")
else()
    set_target_properties(wlx-multidoc-viewer PROPERTIES SUFFIX ".wlx64")
endif()
