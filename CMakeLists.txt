cmake_minimum_required(VERSION 3.20)
project(jp_engine_3d C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ─── hajimu_plugin.h ───────────────────────────────────
find_path(HAJIMU_INCLUDE_DIR hajimu_plugin.h
    PATHS
        ${CMAKE_SOURCE_DIR}/../../jp/include
        ${CMAKE_SOURCE_DIR}/../jp/include
        /usr/local/include/hajimu
    REQUIRED)
message(STATUS "hajimu include: ${HAJIMU_INCLUDE_DIR}")

# ─── SDL2 ──────────────────────────────────────────────
find_package(SDL2 REQUIRED)

# ─── ターゲット ────────────────────────────────────────
set(SRCS
    src/eng_3d.c
    src/plugin.c
)

add_library(engine_3d SHARED ${SRCS})

set_target_properties(engine_3d PROPERTIES
    PREFIX  ""
    SUFFIX  ".hjp"
    OUTPUT_NAME "engine_3d"
)

target_include_directories(engine_3d PRIVATE
    ${HAJIMU_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
    include
    vendor
    src
)

# ─── macOS: OpenGL フレームワーク ─────────────────────
if (APPLE)
    target_compile_definitions(engine_3d PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(engine_3d PRIVATE
        ${SDL2_LIBRARIES}
        "-framework OpenGL"
        "-framework CoreFoundation"
    )
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(engine_3d PRIVATE
        ${SDL2_LIBRARIES}
        OpenGL::GL
        m
    )
endif()

target_compile_options(engine_3d PRIVATE
    -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
)
