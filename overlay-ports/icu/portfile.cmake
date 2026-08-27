vcpkg_download_distfile(
    ARCHIVE
    URLS "https://github.com/unicode-org/icu/releases/download/release-${VERSION}/icu4c-${VERSION}-sources.tgz"
    FILENAME "icu4c-${VERSION}-sources.tgz"
    SHA512 04A49455E1489030C520A4BFD2664FA2171E7938D08F2ACDBBCB1FDA976639FD8B1F0704F2EEC89BA59A7B6D118CEAAB6EC5A096E40D9085A0895D91CE225245
)

vcpkg_extract_source_archive(SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    PATCHES
        disable-static-prefix.patch # https://gitlab.kitware.com/cmake/cmake/-/issues/16617; also mingw.
        fix_bsd_and_solaris.patch
        fix_parallel_build_on_windows.patch
        fix-python-path-with-spaces.patch
        fix-using-install-sh.diff
        mh-darwin.patch
        mh-mingw.patch
        mh-msys-msvc.patch
        subdirs.patch
        vcpkg-cross-data.patch
)

vcpkg_find_acquire_program(PYTHON3)
set(ENV{PYTHON} "${PYTHON3}")

vcpkg_list(SET CONFIGURE_OPTIONS)
vcpkg_list(SET BUILD_OPTIONS)

if(VCPKG_TARGET_IS_EMSCRIPTEN)
    vcpkg_list(APPEND CONFIGURE_OPTIONS --disable-extras icu_cv_host_frag=mh-linux)
    vcpkg_list(APPEND BUILD_OPTIONS "\"PKGDATA_OPTS=--without-assembly -O ../data/icupkg.inc\"")
elseif(VCPKG_TARGET_IS_UWP)
    vcpkg_list(APPEND CONFIGURE_OPTIONS --disable-extras ac_cv_func_tzset=no ac_cv_func__tzset=no)
    string(APPEND VCPKG_C_FLAGS " -DU_PLATFORM_HAS_WINUWP_API=1")
    string(APPEND VCPKG_CXX_FLAGS " -DU_PLATFORM_HAS_WINUWP_API=1")
    vcpkg_list(APPEND BUILD_OPTIONS "\"PKGDATA_OPTS=--windows-uwp-build -O ../data/icupkg.inc\"")
elseif(VCPKG_TARGET_IS_OSX AND VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    vcpkg_list(APPEND CONFIGURE_OPTIONS --enable-rpath)
    if(DEFINED CMAKE_INSTALL_NAME_DIR)
        vcpkg_list(APPEND BUILD_OPTIONS "ID_PREFIX=${CMAKE_INSTALL_NAME_DIR}")
    endif()
endif()

if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
    list(APPEND CONFIGURE_OPTIONS ac_cv_lib_m_floor=no)
endif()

if("tools" IN_LIST FEATURES)
  list(APPEND CONFIGURE_OPTIONS --enable-tools)
else()
  list(APPEND CONFIGURE_OPTIONS --disable-tools)
endif()
if(CMAKE_HOST_WIN32 AND VCPKG_TARGET_IS_MINGW AND NOT HOST_TRIPLET MATCHES "mingw")
    # Assuming no cross compiling because the host (windows) pkgdata tool doesn't
    # use the '/' path separator when creating compiler commands for mingw bash.
elseif(VCPKG_CROSSCOMPILING)
    set(TOOL_PATH "${CURRENT_HOST_INSTALLED_DIR}/tools/${PORT}")
    # convert to unix path
    string(REGEX REPLACE "^([a-zA-Z]):/" "/\\1/" _VCPKG_TOOL_PATH "${TOOL_PATH}")
    list(APPEND CONFIGURE_OPTIONS "--with-cross-build=${_VCPKG_TOOL_PATH}")
endif()

vcpkg_make_configure(
    SOURCE_PATH "${SOURCE_PATH}/source"
    # AUTORECONF # needs Autoconf version 2.72
    OPTIONS
        ${CONFIGURE_OPTIONS}
        --disable-samples
        --disable-tests
        --disable-layoutex
    OPTIONS_RELEASE
        --disable-debug
        --enable-release
    OPTIONS_DEBUG
        --enable-debug
        --disable-release
)
vcpkg_make_install(OPTIONS ${BUILD_OPTIONS})

# Overlay fix: ICU's autotools `make install` (run by vcpkg_make_install with a
# DESTDIR) leaks the entire install under a literal <pkg>/usr/local/ prefix:
# libs (.lib/.dll), headers (.h), pkg-config files and tools land under
# <pkg>/usr/local/{bin,include,lib,share} instead of <pkg>/{bin,include,lib,share}.
# Consumers (pkg-config, Qt's find_package(ICU) wrapper) and later portfile
# steps operating on include/ lib/ operate on the canonical dirs, so we must
# flatten early, before any header/library editing.
function(z_flatten_icu_usr_local _root)
    set(_usr_local "${_root}/usr/local")
    if(NOT EXISTS "${_usr_local}")
        return()
    endif()
    file(GLOB _subdirs "${_usr_local}/*")
    foreach(_sub IN LISTS _subdirs)
        get_filename_component(_subname "${_sub}" NAME)
        if(_subname MATCHES "^(bin|include|lib|share|etc)$")
            file(MAKE_DIRECTORY "${_root}/${_subname}")
            file(COPY "${_sub}/" DESTINATION "${_root}/${_subname}")
        endif()
    endforeach()
    file(REMOVE_RECURSE "${_usr_local}")
endfunction()
z_flatten_icu_usr_local("${CURRENT_PACKAGES_DIR}")
if(NOT VCPKG_BUILD_TYPE)
    z_flatten_icu_usr_local("${CURRENT_PACKAGES_DIR}/debug")
endif()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/share"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/lib/icu"
    "${CURRENT_PACKAGES_DIR}/debug/lib/icu"
    "${CURRENT_PACKAGES_DIR}/debug/lib/icud")

file(GLOB TEST_LIBS
    "${CURRENT_PACKAGES_DIR}/lib/*test*"
    "${CURRENT_PACKAGES_DIR}/debug/lib/*test*")
if(TEST_LIBS)
    file(REMOVE ${TEST_LIBS})
endif()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    # force U_STATIC_IMPLEMENTATION macro
    foreach(HEADER utypes.h utf_old.h platform.h)
        vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/unicode/${HEADER}" "defined(U_STATIC_IMPLEMENTATION)" "1")
    endforeach()
endif()

# Install executables from /tools/icu/sbin to /tools/icu/bin on unix (/bin because icu require this for cross compiling)
if(VCPKG_TARGET_IS_LINUX OR VCPKG_TARGET_IS_OSX AND "tools" IN_LIST FEATURES)
    vcpkg_copy_tools(
        TOOL_NAMES icupkg gennorm2 gencmn genccode gensprep
        SEARCH_DIR "${CURRENT_PACKAGES_DIR}/tools/icu/sbin"
        DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}/bin"
    )
endif()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/tools/icu/sbin"
    "${CURRENT_PACKAGES_DIR}/tools/icu/debug")

# To cross compile, we need some files at specific positions. So lets copy them
file(GLOB CROSS_COMPILE_DEFS "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/config/icucross.*")
file(INSTALL ${CROSS_COMPILE_DEFS} DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}/config")

if(VCPKG_TARGET_IS_WINDOWS)
    string(REGEX MATCH "^[0-9]*" ICU_VERSION_MAJOR "${VERSION}")
    file(GLOB RELEASE_DLLS "${CURRENT_PACKAGES_DIR}/lib/*icu*${ICU_VERSION_MAJOR}.dll")
    file(COPY ${RELEASE_DLLS} DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}/bin")

    # Overlay fix: ICU's Makefile builds the host tools (icupkg, genccode, ...)
    # into the build tree <triplet>-rel/bin but does not install them to
    # tools/icu/bin on Windows (vcpkg_copy_tools only runs on Linux/OSX). The
    # x64-windows-static-md target is cross-built against this host ICU and
    # needs icupkg at x64-windows/tools/icu/bin/icupkg to regenerate data.
    # Copy the host tools + any DLLs they need into tools/icu/bin when building
    # the tools-enabled host flavour (VCPKG_CROSSCOMPILING target).
    if("tools" IN_LIST FEATURES)
        file(GLOB _host_tools "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/bin/*.exe")
        foreach(_t IN LISTS _host_tools)
            file(COPY "${_t}" DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}/bin")
        endforeach()
        file(GLOB _host_dlls "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/bin/*.dll"
                             "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/tools/*/*.dll"
                             "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/lib/*icu*.dll")
        foreach(_d IN LISTS _host_dlls)
            file(COPY "${_d}" DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}/bin")
        endforeach()
    endif()

    # copy dlls
    file(GLOB RELEASE_DLLS "${CURRENT_PACKAGES_DIR}/lib/*icu*${ICU_VERSION_MAJOR}.dll")
    file(COPY ${RELEASE_DLLS} DESTINATION "${CURRENT_PACKAGES_DIR}/bin")
    if(NOT VCPKG_BUILD_TYPE)
        file(GLOB DEBUG_DLLS "${CURRENT_PACKAGES_DIR}/debug/lib/*icu*${ICU_VERSION_MAJOR}.dll")
        file(COPY ${DEBUG_DLLS} DESTINATION "${CURRENT_PACKAGES_DIR}/debug/bin")
    endif()

    # remove any remaining dlls in /lib
    file(GLOB DUMMY_DLLS "${CURRENT_PACKAGES_DIR}/lib/*.dll" "${CURRENT_PACKAGES_DIR}/debug/lib/*.dll")
    if(DUMMY_DLLS)
        file(REMOVE ${DUMMY_DLLS})
    endif()

    vcpkg_copy_pdbs()
endif()

# Overlay fix: ICU's autotools cross/tools build installs the .pc files under
# a literal <pkg>/usr/local/lib/pkgconfig directory (default prefix leaked into
# the pkgconfigdir for the tools-enabled build), so vcpkg_fixup_pkgconfig()
# -- which only looks in lib/pkgconfig and share/pkgconfig -- cannot find them
# and reports BUILD_FAILED. Relocate any strays (should be a no-op after the
# early flatten) before the fixup runs.
function(z_relocate_icu_pkgconfig _root)
    set(_strays "${_root}/usr/local/lib/pkgconfig")
    if(EXISTS "${_strays}")
        file(GLOB _pc_files "${_strays}/*.pc")
        foreach(_pc IN LISTS _pc_files)
            file(COPY "${_pc}" DESTINATION "${_root}/lib/pkgconfig")
        endforeach()
        file(REMOVE_RECURSE "${_root}/usr/local/lib/pkgconfig")
    endif()
endfunction()
z_relocate_icu_pkgconfig("${CURRENT_PACKAGES_DIR}")
if(NOT VCPKG_BUILD_TYPE)
    z_relocate_icu_pkgconfig("${CURRENT_PACKAGES_DIR}/debug")
endif()

vcpkg_fixup_pkgconfig()
set(cxx_link_libraries "")
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    block(PROPAGATE cxx_link_libraries)
        vcpkg_cmake_get_vars(cmake_vars_file)
        include("${cmake_vars_file}")
        list(REMOVE_ITEM VCPKG_DETECTED_CMAKE_CXX_IMPLICIT_LINK_LIBRARIES ${VCPKG_DETECTED_CMAKE_C_IMPLICIT_LINK_LIBRARIES})
        list(TRANSFORM VCPKG_DETECTED_CMAKE_CXX_IMPLICIT_LINK_LIBRARIES REPLACE "^([^/]+)\$" "-l\\1")
        string(JOIN " " cxx_link_libraries ${VCPKG_DETECTED_CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
    endblock()
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/lib/pkgconfig/icu-uc.pc" "baselibs = " "baselibs = ${cxx_link_libraries} ")
    if(NOT VCPKG_BUILD_TYPE)
        if(EXISTS "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/icu-uc.pc")
            vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/icu-uc.pc" "baselibs = " "baselibs = ${cxx_link_libraries} ")
        endif()
    endif()
endif()

if(EXISTS "${CURRENT_PACKAGES_DIR}/tools/icu/bin/icu-config")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/tools/icu/bin/icu-config" "${CURRENT_INSTALLED_DIR}" "`dirname $0`/../../../" IGNORE_UNCHANGED)
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/tools/icu/bin/icu-config" "${CURRENT_HOST_INSTALLED_DIR}" "`dirname $0`/../../../../${_HOST_TRIPLET}/" IGNORE_UNCHANGED)
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-wrapper.cmake" "${CURRENT_PACKAGES_DIR}/share/${PORT}/vcpkg-cmake-wrapper.cmake" @ONLY)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
