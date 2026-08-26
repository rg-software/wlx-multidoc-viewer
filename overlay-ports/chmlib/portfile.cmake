vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

# The builtin vcpkg chmlib port downloads from http://www.jedrea.com, which no
# longer serves the archive (404). This overlay pins the author-maintained
# GitHub mirror instead: jedwing/CHMLib master (= upstream 0.40a), commit-pinned
# for reproducibility.
set(CHMLIB_REF 2bef8d063ec7d88a8de6fd9f0513ea42ac0fa21f)

vcpkg_download_distfile(
    ARCHIVE
    URLS "https://github.com/jedwing/CHMLib/archive/${CHMLIB_REF}.zip"
    FILENAME "jedwing-CHMLib-${CHMLIB_REF}.zip"
    SHA512 9822bb0cf11f7417c00e721e0c4acd297a279ec54bd33b368e6732a911785aa6de4cc0846e5b5e4a685dd066ff1c047ab910c6053b3acc4384a0005f0509c6e1
)

vcpkg_extract_source_archive(
    SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    PATCHES
        strings_h.patch
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS_RELEASE -DBUILD_TOOLS=ON
    OPTIONS_DEBUG -DBUILD_TOOLS=OFF
)

vcpkg_cmake_install()

file(INSTALL "${SOURCE_PATH}/src/chm_lib.h"  DESTINATION "${CURRENT_PACKAGES_DIR}/include")

file(INSTALL "${SOURCE_PATH}/COPYING" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
