
cmake_minimum_required(VERSION 3.16)
project(MaxPayneWrapper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET_NAME "maxpayne_arm64")
set(BUILD_DIR "build")

# ---- Architecture flags ----
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(ARCH_FLAGS "-march=armv8-a")
    set(PLATFORM_DEFINES "-D__LINUX__" "-D__ARM64__")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(ARCH_FLAGS "")
    set(PLATFORM_DEFINES "-D__LINUX__")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O2 -ffunction-sections ${ARCH_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -O2 -ffunction-sections ${ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g ${ARCH_FLAGS} -Wl,-Map,${TARGET_NAME}.map")

# ---- Sources ----
set(SOURCES
    src/main.c
    src/config.c
    src/error.c
    src/gamedata_mapping.c
    src/imports.c
    src/so_util.c
    src/util.c
    src/videoplayer.c
    src/hooks/game.c
    src/hooks/openal.c
    src/hooks/opengl.c
    src/wrappers/drm_wrapper.c
    src/wrappers/gbm_wrapper.c
)

add_executable(${TARGET_NAME} ${SOURCES})
target_compile_definitions(${TARGET_NAME} PRIVATE ${PLATFORM_DEFINES})

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/hooks
)

# ---- Dependencies via pkg-config ----
find_package(PkgConfig REQUIRED)

pkg_check_modules(OPENAL REQUIRED IMPORTED_TARGET openal)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)
pkg_check_modules(GLESV2 REQUIRED IMPORTED_TARGET glesv2)

target_link_libraries(${TARGET_NAME} PRIVATE
    PkgConfig::OPENAL
    PkgConfig::SDL2
    PkgConfig::EGL
    PkgConfig::GLESV2
    m
    dl
    stdc++
    pthread
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${TARGET_NAME} PRIVATE DEBUG=1)
endif()

# ---- Packaging and Archive targets ----
add_custom_target(package
    COMMAND ${CMAKE_COMMAND} -E echo "Creating package..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory package
    COMMAND ${CMAKE_COMMAND} -E make_directory package/maxpayne
    COMMAND ${CMAKE_COMMAND} -E copy ${TARGET_NAME} package/maxpayne/
    COMMAND ${CMAKE_COMMAND} -E copy_directory patch package/maxpayne/patch
    COMMAND ${CMAKE_COMMAND} -E copy "port/Max Payne.sh" "package/Max Payne.sh"
    COMMAND ${CMAKE_COMMAND} -E copy "port/cover.png" "package/cover.png"
    COMMAND ${CMAKE_COMMAND} -E copy "port/gameinfo.xml" "package/gameinfo.xml"
    COMMAND ${CMAKE_COMMAND} -E copy "port/screenshot.png" "package/screenshot.png"
    COMMAND ${CMAKE_COMMAND} -E make_directory package/maxpayne/tools
    COMMAND ${CMAKE_COMMAND} -E copy "port/tools/patchscript" "package/maxpayne/tools/patchscript"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "launcher" "package/maxpayne/launcher"
    COMMAND /bin/sh -c './scripts/generate_version_string.sh > package/maxpayne/VERSION.txt'
    COMMAND /bin/sh -c './scripts/generate_port_json.sh > package/port.json'
    COMMAND /bin/sh -c './scripts/generate_port_readme.sh > package/README.md'
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Creating package"
    DEPENDS ${TARGET_NAME}
)

add_custom_target(archive
    COMMAND ${CMAKE_COMMAND} -E remove_directory archive
    COMMAND ${CMAKE_COMMAND} -E make_directory archive
    COMMAND cd package && zip -r ../archive/maxpayne_arm64.zip *
    DEPENDS package
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Creating archive"
)

add_custom_target(upload-to-device
    COMMAND ./scripts/build_and_upload.sh 192.168.88.97
)

message(STATUS "Build configuration:")
message(STATUS "  Target: ${TARGET_NAME}")
message(STATUS "  Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Using pkg-config for all dependencies")
