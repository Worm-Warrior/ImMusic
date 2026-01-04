cmake_minimum_required(VERSION 3.24)
project(ImMusic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(TagLib CONFIG REQUIRED)
find_package(FFMPEG CONFIG REQUIRED) # via vcpkg
find_package(OpenGL REQUIRED)

add_executable(ImMusic
        main.cpp

        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/imgui_impl_opengl3.cpp

        include/app_state.h
        include/debug_log.h
        include/track.h
        include/audio.h
        include/audio.cpp
        include/file_tree.h

        external/tinyfiledialogs.h
        external/tinyfiledialogs.c
)

target_include_directories(ImMusic PRIVATE
        ${IMGUI_DIR}
)

target_link_libraries(ImMusic PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        CURL::libcurl
        TagLib::TagLib

        FFMPEG::avformat
        FFMPEG::avcodec
        FFMPEG::avutil
        FFMPEG::swresample

        OpenGL::GL
)