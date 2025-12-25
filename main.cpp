#include <iostream>
#include "include/imgui.h"
#include "include/imgui_impl_sdl3.h"
#include "include/imgui_impl_opengl3.h"
#include <SDL3/SDL_surface.h>
#include <cstdio>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "app_state.h"
#include "imgui_internal.h"
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif
#include <filesystem>

// A lot of code taken from the Dear ImGui example for SDL3+opengl3
// Link : https://github.com/ocornut/imgui/blob/master/examples/example_sdl3_opengl3/main.cpp

// This makes it so that the whole window is one big dock space so we get the windows to act the way we want.
void draw_dockspace() {
    ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("DockspaceRoot", nullptr, flags);
    ImGui::DockSpace(ImGui::GetID("MainDockspace"), ImVec2(0, 0));
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void parse_file_system(std::string root_dir) {
    for (const auto &entry: std::filesystem::recursive_directory_iterator(root_dir)) {
        if (entry.is_directory())
            std::printf("Parsed : %s\n", entry.path().string().c_str());
    }
}

static const char *file_type_string(const std::filesystem::directory_entry &e) {
    if (e.is_directory()) {
        return "Folder";
    }
    if (e.is_regular_file()) {
        std::string file_string = e.path().string().substr(e.path().string().find_last_of('.') + 1,
                                                           e.path().string().length());
        return file_string.c_str();
    }

    return "Other";
}

// this will parse and build the UI tree for the root dir!
void build_fs_tree(std::filesystem::path path, ImGuiTreeNodeFlags base) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    bool is_folder = std::filesystem::is_directory(path);

    ImGuiTreeNodeFlags node_flags = base;
    if (!is_folder) {
        node_flags |= ImGuiTreeNodeFlags_Leaf
                | ImGuiTreeNodeFlags_Bullet
                | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    std::string name = path.filename().string();
    // Root node case, we want it to be open, but the subfolders should NOT open by default!
    if (name.empty()) {
        node_flags |= ImGuiTreeNodeFlags_DefaultOpen;
        name = path.string();
    }
    bool open = ImGui::TreeNodeEx(name.c_str(), node_flags);

    ImGui::TableNextColumn();
    if (is_folder) {
        ImGui::TextDisabled("--");
    } else {
        ImGui::Text("%.2lf MB", (double) std::filesystem::file_size(path) * 0.000001);
    }

    ImGui::TableNextColumn();
    ImGui::TextUnformatted(file_type_string(std::filesystem::directory_entry(path)));

    if (is_folder && open) {
        for (const auto &entry: std::filesystem::directory_iterator(path)) {
            build_fs_tree(entry.path().string(), base);
        }
        ImGui::TreePop();
    }
}

int main(int, char **) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }


    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char *glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char *glsl_version = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Set up our SDL window with some flags
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags =
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window *window = SDL_CreateWindow("ImMusic", (int) (1280 * main_scale), (int) (800 * main_scale),
                                          window_flags);
    if (window == nullptr) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Make a GL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        printf("OpenGL context could not be created! SDL Error: %s\n", SDL_GetError());
        return 1;
    }

    // More window config
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // This is vsync
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Set the app icon
    SDL_Surface *icon = IMG_Load("../res/ImMusic.svg");
    if (icon == nullptr) {
        printf("icon was null pointer\n");
    }
    if (!SDL_SetWindowIcon(window, icon)) {
        printf("SDL_SetWindowIcon failed: %s\n", SDL_GetError());
    }

    // Dear ImGui setup context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Again, a more desktop like experience, the user cant nest the windows and shift docking.
    io.ConfigDockingNoDockingOver = true;
    io.ConfigDockingWithShift = true;

    // We can change this to whatever, but it makes it look much better!
    io.Fonts->AddFontFromFileTTF("../res/IosevkaTermNerdFont-Regular.ttf");

    ImGui::StyleColorsDark();

    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    // Make the bg invisible
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0, 0, 0, 0);

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool show_demo_window = true;
    bool show_debug_window = false;
    bool show_file_system_window = true;

    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.15f, 1.0f);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    // This will be our whole app state in one big FAT struct.
    app_state_t app_state;
    app_state.is_running = true;

    // See if this works.
    parse_file_system("/home/harry/Music");

    // MAIN LOOP
    while (app_state.is_running) {
        SDL_Event event;
        // Quit logic from SDL
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                app_state.is_running = false;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                app_state.is_running = false;
            }
        }

        // So don't do anything on a minimized window!
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        // Clear everything out before rendering the frame
        glViewport(0, 0, (int) io.DisplaySize.x, (int) io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        // Start the imgui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Draw the main dock space that will be the base for the app
        draw_dockspace();
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        // Our own simple window
        {
            ImGui::Begin("Local File System", &show_file_system_window);
            static ImGuiTableFlags table_flags =
                    ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

            static ImGuiTreeNodeFlags tree_node_flags_base =
                    ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_DrawLinesFull;


            if (ImGui::BeginTable("file_system", 3, table_flags)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();
                build_fs_tree("/home/harry/Music/", tree_node_flags_base);
                ImGui::EndTable();
            }
            ImGui::End();
        }

        // Make this a debug overlay?
        if (show_debug_window) {
            ImGui::Begin("Debug Window", &show_debug_window);
            // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Average %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // This is where we render all of our draw calls we generated above with the widgets
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
    // Shutdown and cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
