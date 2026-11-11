cmake_minimum_required(VERSION 3.16)
project(uaplay_ps4 LANGUAGES C)

# Cross-compile with PacBrew's OpenOrbis toolchain, never with host libraries.
if(NOT PS4 AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "UAPlay PS4 requires OpenOrbis (source ps4vars.sh; run openorbis-cmake)")
endif()

find_package(PkgConfig REQUIRED)

set(UAPLAY_SOURCES
    ../source/main.c
    ../source/catalog.c
    ../source/posters.c
    ../source/library.c
    ../source/playback.c
)

add_executable(uaplay_ps4 ${UAPLAY_SOURCES})
target_compile_features(uaplay_ps4 PRIVATE c_std_11)
target_compile_options(uaplay_ps4 PRIVATE -O2 -Wall -Wextra)
target_compile_definitions(uaplay_ps4 PRIVATE __PS4__ SDL_MAIN_HANDLED)
target_include_directories(uaplay_ps4 PRIVATE ../source)

# The PacBrew PS4 snapshot has libmpv + FFmpeg, SDL2, SDL2_image, SDL2_ttf,
# and libcurl; this historic snapshot does not provide a libcurl.pc file.
# --static is necessary: the PS4 title does not ship host .so libraries.
execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --static --cflags
            mpv SDL2_ttf SDL2_image
    OUTPUT_VARIABLE PACKAGE_CFLAGS
    RESULT_VARIABLE CFLAGS_OK
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --static --libs
            mpv SDL2_ttf SDL2_image
    OUTPUT_VARIABLE PACKAGE_LIBS
    RESULT_VARIABLE LIBS_OK
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT CFLAGS_OK EQUAL 0 OR NOT LIBS_OK EQUAL 0)
    message(FATAL_ERROR "PS4 PacBrew libraries missing: mpv SDL2_ttf SDL2_image")
endif()
separate_arguments(PACKAGE_CFLAGS NATIVE_COMMAND "${PACKAGE_CFLAGS}")
separate_arguments(PACKAGE_LIBS NATIVE_COMMAND "${PACKAGE_LIBS}")
# PacBrew's CMake link rule runs ld.lld directly, not clang. The -Xlinker
# compiler-driver wrapper is therefore invalid; pass its following option
# straight through to ld.lld instead.
list(REMOVE_ITEM PACKAGE_LIBS "-pthread" "-lpthread" "-Xlinker")
target_compile_options(uaplay_ps4 PRIVATE ${PACKAGE_CFLAGS})
target_link_libraries(uaplay_ps4 PRIVATE
    ${PACKAGE_LIBS} curl
    ScePigletv2VSH ScePrecompiledShaders SceSysmodule SceSystemService
    SceUserService SceVideoOut SceAudioOut ScePad SceNet SceNetCtl SceSsl
    SceHttp SceRtc m
)

# Generated title layout is completely isolated from the PS5 UFS2/ffpkg.
set(PS4_TITLE_DIR "${CMAKE_BINARY_DIR}/package")
file(MAKE_DIRECTORY "${PS4_TITLE_DIR}/sce_sys")
file(MAKE_DIRECTORY "${PS4_TITLE_DIR}/romfs")
file(COPY "${CMAKE_SOURCE_DIR}/../romfs/" DESTINATION "${PS4_TITLE_DIR}/romfs")
file(COPY "${CMAKE_SOURCE_DIR}/../ps5/icon0.png" DESTINATION "${PS4_TITLE_DIR}/sce_sys")

add_self(uaplay_ps4)
add_pkg(uaplay_ps4 "${PS4_TITLE_DIR}" "UAPL00001" "UAPlay" "01.00")
