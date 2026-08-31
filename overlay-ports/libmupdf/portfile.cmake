if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_check_linkage(ONLY_STATIC_LIBRARY) # incomplete DLL exports
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ArtifexSoftware/mupdf
    REF "${VERSION}"
    SHA512 1a05fe0bb4ab7b6841abb4f67890fd14620fe25f0c5a291d9e3514289ef2f9f29ec06e55a35c9a1d4aac8f45a715130df2beedfd06101b535d37f2b2ffa518b2
    HEAD_REF master
)
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/unofficial-libmupdf-config.cmake.in" DESTINATION "${SOURCE_PATH}")

# MuPDF >= 1.28 vendors the mujs regexp/utf C files into libmupdf
# (source/fitz/regexp.c and stext-search.c include them directly), but the
# GitHub tag tarballs ship the thirdparty/mujs submodule empty. Vendor the
# revision MuPDF 1.28.3 pins (ArtifexSoftware/mujs e892c9fd) here.
set(MUJS_REV e892c9fdbbddba94e52f656ccb378ed4885e30cc)
vcpkg_download_distfile(MUJS_REGEXP_C
    URLS "https://raw.githubusercontent.com/ArtifexSoftware/mujs/${MUJS_REV}/regexp.c"
    FILENAME "mujs-${MUJS_REV}-regexp.c"
    SHA512 04d78ceb17258a1c3f4cca3022f5c12934cfd9e43bc6862de690538ab2c6053db5b372a921dbfa44005f39bc60708e16edaa4980aa9ac9650971541988b1a5c6
)
vcpkg_download_distfile(MUJS_REGEXP_H
    URLS "https://raw.githubusercontent.com/ArtifexSoftware/mujs/${MUJS_REV}/regexp.h"
    FILENAME "mujs-${MUJS_REV}-regexp.h"
    SHA512 bda7f31bc3f3a887bd43f6bdb231b30ce187e6fc3c9a25f0904a76b9727d64595371248d703442d1b088c73f99559e31e13947a9f112efc4b3801e9b7178c0b2
)
vcpkg_download_distfile(MUJS_UTF_C
    URLS "https://raw.githubusercontent.com/ArtifexSoftware/mujs/${MUJS_REV}/utf.c"
    FILENAME "mujs-${MUJS_REV}-utf.c"
    SHA512 f0ed4d21857d605208029a28ee4f15a3133c990695d889f63849bc665d5b010f96249fb588a931608ec22ceb48ba3bbfd9d7c068a808c2d8162b2cf412428e64
)
vcpkg_download_distfile(MUJS_UTF_H
    URLS "https://raw.githubusercontent.com/ArtifexSoftware/mujs/${MUJS_REV}/utf.h"
    FILENAME "mujs-${MUJS_REV}-utf.h"
    SHA512 a0ff33407095f3b3729edc731c4ee6c57ec7dc176cf5dde4bd05e73f21aaa98be80637438f4b902abd5ba016a9b7a7239193bdfc83e5d66fd4b5992d68b01526
)
vcpkg_download_distfile(MUJS_UTFDATA_H
    URLS "https://raw.githubusercontent.com/ArtifexSoftware/mujs/${MUJS_REV}/utfdata.h"
    FILENAME "mujs-${MUJS_REV}-utfdata.h"
    SHA512 8e8a30e97e0ddc9110f563dd1c3ab13ce1af429f1aa3b55cacf22f884bf807582cea19ae8d52d901d8d1cac3ee2903dfd8dd89d633a0e2ed7a4092826e0e6f3f
)
file(COPY_FILE "${MUJS_REGEXP_C}" "${SOURCE_PATH}/thirdparty/mujs/regexp.c")
file(COPY_FILE "${MUJS_REGEXP_H}" "${SOURCE_PATH}/thirdparty/mujs/regexp.h")
file(COPY_FILE "${MUJS_UTF_C}" "${SOURCE_PATH}/thirdparty/mujs/utf.c")
file(COPY_FILE "${MUJS_UTF_H}" "${SOURCE_PATH}/thirdparty/mujs/utf.h")
file(COPY_FILE "${MUJS_UTFDATA_H}" "${SOURCE_PATH}/thirdparty/mujs/utfdata.h")

vcpkg_check_features(
    OUT_FEATURE_OPTIONS OPTIONS
    FEATURES
        ocr ENABLE_OCR
)

if(VCPKG_CROSSCOMPILING AND VCPKG_HOST_IS_WINDOWS AND VCPKG_TARGET_IS_WINDOWS)
    list(APPEND OPTIONS "-DBIN2COFF_EXECUTABLE=${CURRENT_HOST_INSTALLED_DIR}/manual-tools/${PORT}/bin2coff.exe")
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${OPTIONS}
)
vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME "unofficial-libmupdf")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/manual-tools")

set(font_licenses "")
foreach(item IN ITEMS urw/OFL.txt noto/COPYING han/LICENSE.txt droid/NOTICE sil/OFL.txt)
    string(REPLACE "/" " " new_name "# Fonts - ${item}")
    set(file "${CURRENT_BUILDTREES_DIR}/${new_name}")
    file(COPY_FILE "${SOURCE_PATH}/resources/fonts/${item}" "${file}")
    list(APPEND font_licenses "${file}")
endforeach()

vcpkg_install_copyright(
    # Cf. source/fitz/noto.c
    COMMENT [[
This software includes Base 14 PDF fonts from URW, Noto fonts from Google.
Source Han Serif from Adobe for CJK, DroidSansFallback from Android for CJK,
Charis SIL from SIL.
]]
    FILE_LIST
        "${SOURCE_PATH}/COPYING"
        ${font_licenses}
)
