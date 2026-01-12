cmake_minimum_required(VERSION 3.24)
project(ImMusic LANGUAGES CXX C)  # Add C language support

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(TagLib CONFIG REQUIRED)
find_package(OpenGL REQUIRED)


# Manually find FFmpeg libraries
find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY NAMES avcodec)
find_library(AVFORMAT_LIBRARY NAMES avformat)
find_library(AVUTIL_LIBRARY NAMES avutil)
find_library(SWRESAMPLE_LIBRARY NAMES swresample)

# Check if libraries were found
if(NOT AVCODEC_LIBRARY OR NOT AVFORMAT_LIBRARY OR NOT AVUTIL_LIBRARY OR NOT SWRESAMPLE_LIBRARY)
    message(FATAL_ERROR "FFmpeg libraries not found!")
endif()

add_executable(ImMusic
        main.cpp

        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/imgui_impl_opengl3.cpp
        external/simdjson.cpp

        include/app_state.h
        include/debug_log.h
        include/track.h
        include/audio.h
        include/audio.cpp
        include/file_tree.h

        ${IMGUI_DIR}/tinyfiledialogs.h
        ${IMGUI_DIR}/tinyfiledialogs.c
)

target_include_directories(ImMusic PRIVATE
        ${IMGUI_DIR}
        ${FFMPEG_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ImMusic PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        CURL::libcurl
        TagLib::TagLib
        OpenGL::GL
        ${AVCODEC_LIBRARY}
        ${AVFORMAT_LIBRARY}
        ${AVUTIL_LIBRARY}
        ${SWRESAMPLE_LIBRARY}
)