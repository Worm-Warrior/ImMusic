#include <iostream>
#include "external/imgui.h"
#include "external/imgui_impl_sdl3.h"
#include "external/imgui_impl_opengl3.h"
#include <SDL3/SDL_surface.h>
#include <cstdio>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "include/app_state.h"
#include "external/imgui_internal.h"
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif
#include <filesystem>
#include "include/debug_log.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <algorithm>

static constexpr std::string_view valid_formats[] = {
    ".mp3", ".flac", ".wav", ".ogg", ".opus", ".aac", ".m4a"
};

/* *
 * A lot of code taken from the Dear ImGui example for SDL3+opengl3 and the demo file
 * Link : https://github.com/ocornut/imgui/blob/master/examples/example_sdl3_opengl3/main.cpp
 * Demo file found in : include/imgui_demo.cpp
 * */

// * This will be our whole app state in one big FAT GLOBAL struct.
static app_state_t app_state;
// * This is also a global for logging anything in this file!
static app_log debug_log;

// This makes it so that the whole window is one big dock space so we get the windows to act the way we want.
// ? Should I move this to another file or is it fine here?
void draw_dockspace() {
    ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDocking |
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

    ImGui::Begin("Main View", nullptr, flags);
    ImGui::DockSpace(ImGui::GetID("MainDockspace"), ImVec2(0, 0));
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

/*
 * === FILE SYSTEM PARSING ===
 * Build our local browser for folders / albums
 *
 * Ideas for later:
 * We should be caching this on first load of a new root folder (we can't now, but will be important when the user can).
 * If it gets too bad for the frame time on first load -> maybe over more than one frame?
 * Try and keep the data structure flat, like in the imgui_demo.cpp example.
 */

// ? Move this filesystem stuff to its own system_file header / cpp file ?

// ! TODO: CACHE THIS, PERF BAD
static std::string file_type_string(const std::filesystem::directory_entry &e) {
    if (e.is_directory()) {
        return "Folder";
    }
    if (e.is_regular_file()) {
        std::string file_string = e.path().string().substr(e.path().string().find_last_of('.') + 1,
                                                           e.path().string().length());
        return file_string;
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

    if (std::filesystem::is_directory(path) && (ImGui::IsItemClicked() || ImGui::IsItemActivated()) && path != app_state
        .
        cur_selected_folder) {
        debug_log.AddLog("[INFO]: cur_selected_folder: %s\n", path.string().c_str());
        app_state.cur_selected_folder = path;
    }

    ImGui::TableNextColumn();
    if (is_folder) {
        ImGui::TextDisabled("--");
    } else {
        ImGui::Text("%.2lf MB", (double) std::filesystem::file_size(path) * 0.000001);
    }

    ImGui::TableNextColumn();
    const std::string type = file_type_string(std::filesystem::directory_entry(path));
    ImGui::TextUnformatted(type.c_str());

    if (is_folder && open) {
        for (const auto &entry: std::filesystem::directory_iterator(path)) {
            build_fs_tree(entry.path().string(), base);
        }
        ImGui::TreePop();
    }
}

/*
 * === MEDIA VIEW ===
 * This will build up our media view.
 *
 * Ideas for later:
 * Use the clipping from ImGui to load only sections at a time.
 * Spread the rendering across more than one frame, so we don't get frame time spikes.
 * Maybe pass something smaller than the whole path of the file to the ImGui::Selectable().
 */

void build_media_view(std::filesystem::path path) {
    for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(path)) {
        if (std::filesystem::is_directory(entry)) {
            build_media_view(entry);
        } else {
            // Awful C++ string manipulations
            std::string ext = entry.path().extension().string();

            for (char &c: ext) {
                if (c >= 'A' && c <= 'Z') {
                    c += 'a' - 'A';
                }
            }

            bool is_valid = false;
            for (auto e: valid_formats) {
                if (ext == e) {
                    is_valid = true;
                    break;
                }
            }

            if (!is_valid) {
                continue;
            }
            TagLib::FileRef f((entry.path().string().c_str()));
            track_t t;
            t.path = entry.path();
            t.is_remote = false;
            t.track_number = f.tag()->track();
            t.track_name = std::string(f.tag()->title().to8Bit());
            t.artist_name = std::string(f.tag()->artist().to8Bit());
            t.album_name = std::string(f.tag()->album().to8Bit());
            t.duration = f.audioProperties()->lengthInSeconds();
            app_state.media_view_tracks.push_back(t);
            debug_log.AddLog("[INFO]: added track %s to the media_view_tracks\n", t.track_name.c_str());
        }
    }
    app_state.cur_media_folder = path;
}

void display_tracks(const std::vector<track_t> &tracks) {
    for (const track_t &t: tracks) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool is_selected = (app_state.cur_selected_track == t.path);

        if ((ImGui::Selectable(("##" + t.path.string()).c_str(), is_selected,
                               ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) &&
            ImGui::IsMouseDoubleClicked(0)) {
            app_state.cur_selected_track = t.path;
            debug_log.AddLog("[INFO]: cur_selected_track: %s\n", t.path.c_str());
        }

        ImGui::SameLine();
        ImGui::Text("%d", t.track_number);
        ImGui::TableNextColumn();
        ImGui::Text("%s", t.track_name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%s", t.artist_name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%s", t.album_name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d", t.duration);
    }
}

void show_media_view(std::filesystem::path path) {
    if (path.empty()) {
        app_state.show_media_view = false;
        return;
    }

    ImGui::Begin("Media View", &app_state.show_media_view);
    ImGuiTableFlags media_table_flags =
            ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Hideable |
            ImGuiTableFlags_Sortable;

    if (ImGui::BeginTable("media_view", 5, media_table_flags)) {
        ImGui::TableSetupColumn(
            "Track",
            ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortAscending |
            ImGuiTableColumnFlags_DefaultSort, 80.0f);
        ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Artist", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Album", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        display_tracks(app_state.media_view_tracks);
        ImGui::EndTable();
    }
    ImGui::End();
}

// * === ENTRY POINT ===
int main(int, char **) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }


    // * Decide GL+GLSL versions based on platform
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

    // * Make this a font that handles special chars
    io.Fonts->AddFontFromFileTTF("../res/MPLUS1Code-Regular.ttf");

    ImGui::StyleColorsDark();

    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    // Make the bg invisible
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0, 0, 0, 0);

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool show_demo_window = false;
    bool show_log = false;


    bool show_file_system_window = true;

    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.15f, 1.0f);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    app_state.is_running = true;

    // * === MAIN LOOP ===
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

        if (ImGui::IsKeyReleased(ImGuiKey_F1)) {
            show_demo_window ^= true;
        }

        if (ImGui::IsKeyReleased(ImGuiKey_F2)) {
            show_log ^= true;
        }

        // * This is the file browser window stuff
        {
            ImGui::Begin("Local File System", &show_file_system_window);
            // TODO: Refactor out to a function?
            static ImGuiTableFlags table_flags =
                    ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

            static ImGuiTreeNodeFlags tree_node_flags_base =
                    ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_DrawLinesFull |
                    ImGuiTreeNodeFlags_OpenOnDoubleClick;


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

        /*
         * If the current selected folder is NOT the same as the current media folder, then we need to rebuild the
         * media player!
        */
        if (app_state.cur_selected_folder != app_state.cur_media_folder) {
            // Clear the old tracks before we rebuild the list!
            app_state.media_view_tracks.clear();
            build_media_view(app_state.cur_selected_folder);
            debug_log.AddLog("[INFO]: %lu tracks in the list\n", app_state.media_view_tracks.size());
        }
        show_media_view(app_state.cur_selected_folder);

        // * Simple log console copied from the examples in /include/imgui_demo.cpp
        if (show_log) {
            ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
            debug_log.Draw("Debug Log", &show_log);
        }

        {
            ImGui::Begin("frametime");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
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
