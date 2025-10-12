cmake_minimum_required(VERSION 3.16)
project(MaxPayneWrapper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_X11_DESKTOP "Build X11 desktop version instead of device version" OFF)

set(TARGET_NAME "maxpayne_arm64")
set(BUILD_DIR "build")

if(BUILD_X11_DESKTOP)
    set(TARGET_NAME "maxpayne_x11")
    set(BUILD_DIR "build_x11")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(ARCH_FLAGS "-march=armv8-a")
    set(PLATFORM_DEFINES "-D__LINUX__" "-D__ARM64__")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(ARCH_FLAGS "")
    set(PLATFORM_DEFINES "-D__LINUX__")
endif()

if(BUILD_X11_DESKTOP)
    list(APPEND PLATFORM_DEFINES "-D__X11_DESKTOP__")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O2 -ffunction-sections ${ARCH_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -O2 -ffunction-sections ${ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g ${ARCH_FLAGS} -Wl,-Map,${TARGET_NAME}.map")

file(GLOB_RECURSE SOURCES
    "src/*.c"
    "src/*.cpp"
)

# Explicitly include all source files to ensure nothing is missed
set(SOURCES
    src/main.c
    src/config.c
    src/error.c
    src/imports.c
    src/so_util.c
    src/util.c
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
    /usr/include
    /usr/local/include
)

if(BUILD_X11_DESKTOP)
    target_include_directories(${TARGET_NAME} PRIVATE
        /usr/include/SDL2
    )

    target_link_libraries(${TARGET_NAME}
        openal
        SDL2
        GL
        EGL
        X11
        m
        dl
        stdc++
        pthread
    )
else()
    target_include_directories(${TARGET_NAME} PRIVATE
        /usr/include/aarch64-linux-gnu
        /usr/include/drm
        /usr/include/libdrm
    )

    target_link_libraries(${TARGET_NAME}
        openal
        SDL2
        EGL
        GLESv2
        m
        dl
        stdc++
        pthread
    )
endif()

target_link_directories(${TARGET_NAME} PRIVATE
    /usr/lib/aarch64-linux-gnu
    /usr/local/lib
)

add_custom_target(build-in-docker
    COMMAND docker run --volume .:/home/project --platform=linux/arm64 --workdir=/home/project monkeyx/retro_builder:arm64 ./docker_cmake_build.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building in Docker container"
)

add_custom_target(upload-to-device
    COMMAND rsync -aP --rsh="sshpass -p ark ssh -o StrictHostKeyChecking=no -l ark" maxpayne_arm64 ark@192.168.88.97:/roms/ports/maxpayne/maxpayne_arm64
    DEPENDS build-in-docker
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Uploading binary to device"
)

add_custom_target(package
    COMMAND ${CMAKE_COMMAND} -E echo "Creating package from Docker build..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory package
    COMMAND ${CMAKE_COMMAND} -E make_directory package/maxpayne
    COMMAND ${CMAKE_COMMAND} -E copy README.md package/maxpayne/README.md
    COMMAND ${CMAKE_COMMAND} -E copy ${TARGET_NAME} package/maxpayne/
    COMMAND ${CMAKE_COMMAND} -E copy_directory patch package/maxpayne/patch
    COMMAND ${CMAKE_COMMAND} -E copy "port/Max Payne.sh" "package/Max Payne.sh"
    COMMAND ${CMAKE_COMMAND} -E copy "port/port.json" "package/maxpayne/port.json"
    COMMAND ${CMAKE_COMMAND} -E make_directory package/maxpayne/tools
    COMMAND ${CMAKE_COMMAND} -E copy "port/tools/patchscript" "package/maxpayne/tools/patchscript"
    DEPENDS build-in-docker
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Creating package from Docker build"
)

add_custom_target(archive
    COMMAND ${CMAKE_COMMAND} -E remove_directory archive
    COMMAND ${CMAKE_COMMAND} -E make_directory archive
    COMMAND cd package && zip -r ../archive/maxpayne_arm64.zip *
    DEPENDS package
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Creating archive from Docker build"
)

add_custom_target(upload-package-to-device
    COMMAND rsync -aP --rsh="sshpass -p ark ssh -o StrictHostKeyChecking=no -l ark" package/ ark@192.168.88.97:/roms/ports/
    DEPENDS package
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Uploading package to device"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${TARGET_NAME} PRIVATE DEBUG=1)
endif()

message(STATUS "Build configuration:")
message(STATUS "  Target: ${TARGET_NAME}")
message(STATUS "  Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  X11 Desktop: ${BUILD_X11_DESKTOP}")
message(STATUS "  Platform defines: ${PLATFORM_DEFINES}")
