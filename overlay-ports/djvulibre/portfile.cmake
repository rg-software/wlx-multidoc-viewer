set(VCPKG_POLICY_EMPTY_PACKAGE enabled)

vcpkg_from_sourceforge(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO djvu/DjVuLibre
    FILENAME "djvulibre-3.5.28.tar.gz"
    REF 3.5.28
    SHA512 db3b8a5b56d700e911be32057f721a2a597e6f52e6fade203ad75ad76ab2d8facff2e474fd18beea703ccd5fa6425352e619a8fda40e69add1724dbee26050c6
)

file(WRITE "${SOURCE_PATH}/libdjvu/CMakeLists.txt" [===[
cmake_minimum_required(VERSION 3.15)
project(libdjvu LANGUAGES C CXX)

file(GLOB DJVU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(djvulibre STATIC ${DJVU_SOURCES})

target_include_directories(djvulibre PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(djvulibre PUBLIC "DJVUAPI=" "DDJVUAPI=" "MINILISPAPI=")

if(MSVC)
    target_compile_definitions(djvulibre PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(djvulibre PRIVATE /W3)
else()
    target_compile_options(djvulibre PRIVATE -Wall -Wextra)
endif()

set_target_properties(djvulibre PROPERTIES
    CXX_STANDARD 17
    C_STANDARD 99
)

install(TARGETS djvulibre ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/ddjvuapi.h" DESTINATION include/libdjvu)
]===])

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/libdjvu"
    OPTIONS_DEBUG
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL
    OPTIONS_RELEASE
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
)

vcpkg_cmake_install()

# Install a simple CMake config file for find_package support
set(_config_dir "${CURRENT_PACKAGES_DIR}/share/djvulibre")
file(MAKE_DIRECTORY "${_config_dir}")
file(WRITE "${_config_dir}/djvulibre-config.cmake" [===[
# DjVuLibre CMake config file
get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}/../../" ABSOLUTE)

if(NOT TARGET djvulibre::djvulibre)
    add_library(djvulibre::djvulibre UNKNOWN IMPORTED)

    set_target_properties(djvulibre::djvulibre PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "DJVUAPI=;DDJVUAPI=;MINILISPAPI="
        INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"
    )

    # Debug library
    find_library(_djvulibre_debug NAMES djvulibre
        PATHS "${_IMPORT_PREFIX}/debug/lib"
        NO_DEFAULT_PATH
    )
    # Release library
    find_library(_djvulibre_release NAMES djvulibre
        PATHS "${_IMPORT_PREFIX}/lib"
        NO_DEFAULT_PATH
    )

    if(_djvulibre_debug AND _djvulibre_release)
        set_target_properties(djvulibre::djvulibre PROPERTIES
            IMPORTED_LOCATION_DEBUG "${_djvulibre_debug}"
            IMPORTED_LOCATION_RELEASE "${_djvulibre_release}"
            IMPORTED_LOCATION "${_djvulibre_release}"
        )
    elseif(_djvulibre_release)
        set_target_properties(djvulibre::djvulibre PROPERTIES
            IMPORTED_LOCATION "${_djvulibre_release}"
        )
    elseif(_djvulibre_debug)
        set_target_properties(djvulibre::djvulibre PROPERTIES
            IMPORTED_LOCATION "${_djvulibre_debug}"
            IMPORTED_LOCATION_DEBUG "${_djvulibre_debug}"
        )
    endif()
endif()

unset(_IMPORT_PREFIX)
unset(_djvulibre_debug)
unset(_djvulibre_release)
]===])

file(WRITE "${_config_dir}/djvulibre-config-version.cmake" [=[
set(PACKAGE_VERSION "3.5.28")
if("${PACKAGE_FIND_VERSION}" VERSION_GREATER "3.5.28")
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if("${PACKAGE_FIND_VERSION}" VERSION_EQUAL "3.5.28")
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
]=])

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
