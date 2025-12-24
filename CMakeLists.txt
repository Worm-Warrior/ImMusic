cmake_minimum_required(VERSION 4.1)
project(ImMusic)

set(CMAKE_CXX_STANDARD 20)

set(IMGUI_DIR include)

find_package(SDL3 REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(VLC REQUIRED libvlc)
pkg_check_modules(TAGLIB REQUIRED taglib)

add_executable(ImMusic
        main.cpp
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/imgui_impl_opengl3.cpp
)

target_include_directories(ImMusic PRIVATE
        ${IMGUI_DIR}
        ${VLC_INCLUDE_DIRS}
        ${TAGLIB_INCLUDE_DIRS}
        /usr/include/SDL3        # SDL3 headers
        /usr/include/SDL3/SDL3_image.h  # SDL3_image header location
)

target_link_libraries(
        ImMusic
        SDL3::SDL3
        /usr/lib64/libSDL3_image.so   # manually link SDL3_image
        CURL::libcurl
        ${VLC_LIBRARIES}
        ${TAGLIB_LIBRARIES}
        GL
)
