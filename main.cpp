/* *
 * A lot of the setup code taken from the Dear ImGui example for SDL3+opengl3 and the demo file
 * Link : https://github.com/ocornut/imgui/blob/master/examples/example_sdl3_opengl3/main.cpp
 * Demo file found in : include/imgui_demo.cpp
 * */

#include <chrono>
#include <iostream>
#include "external/imgui.h"
#include "external/imgui_impl_sdl3.h"
#include "external/imgui_impl_opengl3.h"
#include <SDL3/SDL_surface.h>
#include <cstdio>
#include <format>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "include/app_state.h"
#include "external/imgui_internal.h"
#include "include/audio.h"
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
#include "include/file_tree.h"
#include "external/tinyfiledialogs.h"

extern "C" {
#include <ffmpeg/libavcodec/avcodec.h>
}

static constexpr std::string_view valid_formats[] = {
    ".mp3", ".flac", ".wav", ".ogg", ".opus", ".aac", ".m4a"
};

// TODO: this needs a big refactor to keep the main.cpp small and more readable.


// * This will be our whole app state in one big FAT GLOBAL struct.
static app_state_t app_state;
// * This is also a global for logging anything in this file!
static app_log debug_log;
// * Thread that we use to decode.
static std::thread decoder_thread;
// * Audio context;
static audio_context_t audio_context;
// * We know if we need to init audio
static bool has_audio_init = false;
// * Cache of the filesystem
static file_system_cache_t file_system_cache;

// This makes it so that the whole window is one big dock space so we get the windows to act the way we want.
// ? Should I move this to another file or is it fine here?
void draw_dockspace() {
    ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDecoration;

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("Main View", nullptr, flags);

    // Main menu bar logic goes here.
    // TODO: Add the dropdown menus I want and code the logic.
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Menu")) {
            if (ImGui::MenuItem("Change root directory")) {
                // Holy moly this is so simple and just works like this, thank God for OSS and platform layers!
                const char *path = tinyfd_selectFolderDialog("Select Music Folder",
                                                             app_state.cur_root_dir.c_str());
                if (path) {
                    app_state.new_root_dir = std::string(path);
                }
            }
            if (ImGui::MenuItem("Search")) {
            }
            if (ImGui::MenuItem("Exit")) { app_state.is_running = false; }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::DockSpace(ImGui::GetID("MainDockspace"), ImVec2(0, 0));
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}




/* *
 * === FILE SYSTEM PARSING ===
 * Build our local browser for folders / albums
 *
 * Ideas for later:
 * If it gets too bad for the frame time on first load -> maybe over more than one frame?
 * */

// ? Move this filesystem stuff to its own system_file header / cpp file ?

void add_folder_rec(const std::filesystem::path &root, int parent_idx, file_system_cache_t &cache) {
    for (const auto &entry: std::filesystem::directory_iterator(root)) {
        if (entry.is_directory()) {
            int idx = cache.nodes.size();
            cache.nodes[parent_idx].child_indexes.push_back(idx);

            file_tree_node_t node;
            node.name = entry.path().filename().string();
            node.path = entry.path();
            node.parent_index = parent_idx;

            cache.nodes.push_back(node);

            add_folder_rec(node.path, idx, cache);

            cache.nodes[idx].has_children = !cache.nodes[idx].child_indexes.empty();
        }
    }
}

void render_node(int node_idx, const file_system_cache_t &cache) {
    const file_tree_node_t &node = cache.nodes[node_idx];

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_OpenOnArrow;

    if (node_idx == 0) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
    }

    if (!node.has_children) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);

    if (ImGui::IsItemClicked() && node.path != app_state.cur_selected_folder) {
        app_state.cur_selected_folder = node.path;
        debug_log.AddLog("[INFO]: Selected folder %s\n", node.path.c_str());
        app_state.show_media_view = true;
    }

    ImGui::TableNextColumn();
    ImGui::Text("Folder");

    if (open) {
        for (int i: node.child_indexes) {
            render_node(i, cache);
        }
        ImGui::TreePop();
    }
}

void scan_folders(const std::filesystem::path &root, file_system_cache_t &cache) {
    debug_log.AddLog("[INFO]: Rebuilding root: %s\n", root.c_str());
    cache.nodes.clear();
    cache.root = root;

    file_tree_node_t root_node;
    root_node.name = root.string();
    root_node.parent_index = -1;
    root_node.path = root;
    cache.nodes.push_back(root_node);

    add_folder_rec(root, 0, cache);

    app_state.cur_root_dir = root;
}

void load_and_play_file(const track_t &track) {
    if (decoder_thread.joinable()) {
        audio_context.should_stop = true;
        decoder_thread.join();
    }

    std::filesystem::path track_path = track.path;

    if (!load_file(audio_context, track_path.string())) {
        debug_log.AddLog("[ERROR]: Failed to load file %s\n", track_path.string().c_str());
        return;
    }
    debug_log.AddLog("[INFO]: Loaded file: %s\n", track_path.string().c_str());

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = audio_context.codec_context->sample_rate;
    spec.channels = audio_context.codec_context->ch_layout.nb_channels;
    spec.format = SDL_AUDIO_F32;
    audio_context.bytes_per_frame = SDL_AUDIO_FRAMESIZE(spec);

    if (audio_context.audio_stream) {
        SDL_DestroyAudioStream(audio_context.audio_stream);
    }

    if (!init_audio(audio_context, spec)) {
        debug_log.AddLog("[ERROR]: Failed to init audio\n");
        return;
    }
    has_audio_init = true;

    SDL_AudioDeviceID device = SDL_GetAudioStreamDevice(audio_context.audio_stream);
    SDL_SetAudioDeviceGain(device, 0.5f);

    audio_context.should_stop = false;
    audio_context.is_paused = false;
    start_decoding_thread(audio_context, decoder_thread, app_state);
    app_state.seek_time = 0;
    app_state.seek_max = track.duration.count();
    debug_log.AddLog("[INFO]: Now playing: %s\n", track_path.filename().string().c_str());
}

/* *
 * === MEDIA VIEW ===
 * This will build up our media view.
 *
 * Ideas for later:
 * Use the clipping from ImGui to load only sections at a time.
 * Spread the rendering across more than one frame, so we don't get frame time spikes.
 * Maybe pass something smaller than the whole path of the file to the ImGui::Selectable().
 * */

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

            // Filter the valid file formats we want to play.
            bool is_valid = false;
            for (auto e: valid_formats) {
                if (ext == e) {
                    is_valid = true;
                    break;
                }
            }
            // If the file is not a valid format -> skip it.
            if (!is_valid) {
                continue;
            }
#ifdef _WIN32
            TagLib::FileRef f((entry.path().wstring().c_str()));
#else
            TagLib::FileRef f((entry.path().string().c_str()));
#endif
            track_t t;
            t.path = entry.path();
            t.track_number = f.tag()->track();
            // Need true for unicode, else the asian characters don't show right on media view.
            t.track_name = std::string(f.tag()->title().to8Bit(true));
            if (t.track_name.empty()) {
                t.track_name = "Unknown";
            }
            t.artist_name = std::string(f.tag()->artist().to8Bit(true));
            if (t.artist_name.empty()) {
                t.artist_name = "Unknown";
            }
            t.album_name = std::string(f.tag()->album().to8Bit(true));
            if (t.album_name.empty()) {
                t.album_name = "Unknown";
            }
            t.duration = std::chrono::seconds(f.audioProperties()->lengthInSeconds());
            app_state.media_view_tracks.push_back(t);
            debug_log.AddLog("[INFO]: added track %s to the media_view_tracks\n", t.track_name.c_str());
        }
    }
    app_state.cur_media_folder = path;
}

void display_tracks(const std::vector<track_t> &tracks) {
    ImGuiListClipper clipper;
    clipper.Begin(tracks.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const track_t &t = tracks[i];
            bool is_selected = (app_state.cur_selected_track.path == t.path);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if ((ImGui::Selectable(("##" + t.path.string()).c_str(), is_selected,
                                   ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) &&
                ImGui::IsMouseDoubleClicked(0)) {
                app_state.playing_tracks = app_state.media_view_tracks;
                // Update the state so we have the right index(es).
                app_state.cur_selected_track = t;
                uint32_t index = 0;
                for (const track_t &track: app_state.playing_tracks) {
                    if (track.path == t.path) {
                        break;
                    }
                    index++;
                }
                if (index < tracks.size()-1) {
                    app_state.should_play_next = true;
                } else {
                    app_state.should_play_next = false;
                }
                app_state.cur_track_index = index;
                load_and_play_file(t);
                app_state.seek_max = t.duration.count();
                debug_log.AddLog("[INFO]: cur_selected_track: %s at index %d\n", t.path.c_str(), index);
            }
            ImGui::SameLine();
            ImGui::Text("%d", t.track_number);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", t.track_name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", t.artist_name.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", t.album_name.c_str());
            ImGui::TableSetColumnIndex(4);

            auto dur = t.duration;

            auto mm = duration_cast<std::chrono::minutes>(dur);
            auto ss = dur - mm;
            ImGui::Text("%02lld:%02lld",
                        static_cast<long long>(mm.count()),
                        static_cast<long long>(ss.count()));
        }
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
            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;

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

/* *
 * === PLAYER CONTROLS ==
 * */

void draw_player_controls() {
    ImGui::Begin("Player Control");
    ImGui::LabelText("", "%s", app_state.cur_selected_track.track_name.c_str());
    if (ImGui::Button("Prev")) {
        if (app_state.cur_track_index - 1 < app_state.playing_tracks.size()) {
            app_state.cur_track_index--;
            app_state.cur_selected_track = app_state.playing_tracks[app_state.cur_track_index];
            load_and_play_file(app_state.playing_tracks[app_state.cur_track_index]);
            debug_log.AddLog("[INFO]: Prev playing: %s\n", app_state.cur_selected_track.track_name.c_str());
        } else {
            app_state.cur_track_index = app_state.playing_tracks.size() - 1;
            app_state.cur_selected_track = app_state.playing_tracks[app_state.cur_track_index];
            load_and_play_file(app_state.playing_tracks[app_state.cur_track_index]);
            debug_log.AddLog("[INFO]: No prev track looping to back to end: %s\n",
                             app_state.cur_selected_track.track_name.c_str());
        }
    }

    ImGui::SameLine();
    if (audio_context.is_paused) {
        if (ImGui::Button("Play###PlayPause")) {
            debug_log.AddLog("Pressed Play\n");
            toggle_play_pause(audio_context);
        }
    } else {
        if (ImGui::Button("Pause###PlayPause")) {
            debug_log.AddLog("Pressed Pause\n");
            toggle_play_pause(audio_context);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Next")) {
        if (app_state.cur_track_index + 1 < app_state.playing_tracks.size()) {
            app_state.cur_track_index++;
            app_state.cur_selected_track = app_state.playing_tracks[app_state.cur_track_index];
            load_and_play_file(app_state.playing_tracks[app_state.cur_track_index]);
            debug_log.AddLog("[INFO]: Next playing: %s\n", app_state.cur_selected_track.track_name.c_str());
        } else {
            app_state.cur_track_index = 0;
            app_state.cur_selected_track = app_state.playing_tracks[app_state.cur_track_index];
            load_and_play_file(app_state.playing_tracks[app_state.cur_track_index]);
            debug_log.AddLog("[INFO]: No next track looping back to start: %s\n",
                             app_state.cur_selected_track.track_name.c_str());
        }
    }
    if (ImGui::SliderInt("Time", &app_state.seek_time, app_state.seek_min, app_state.seek_max)) {
        app_state.is_seeking = true;
    } else if (ImGui::IsItemDeactivated() && app_state.is_seeking != false) {
        debug_log.AddLog("Seeking released at: %d\n", app_state.seek_time);
        app_state.is_seeking = false;
        app_state.seek_queued = true;
        // * This is here because if we don't delay by a frame we can get garbage, so delay a frame.
        app_state.just_seeked.exchange(true);
        audio_context.seek_seconds.store(app_state.seek_time, std::memory_order_relaxed);
        audio_context.seek_req.store(true, std::memory_order_release);
    }
    if (ImGui::SliderFloat("Volume", &app_state.cur_track_volume, 0, 1, "%.2f")) {
        debug_log.AddLog("Volume: %f\n", app_state.cur_track_volume);
    }
    ImGui::End();
    // TODO: make this update only if the volume has changed.
    SDL_SetAudioDeviceGain(SDL_GetAudioStreamDevice(audio_context.audio_stream),
                           app_state.cur_track_volume);
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

    app_state.new_root_dir = "/home/harry/Music";

    // Make the bg invisible
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0, 0, 0, 0);

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool show_demo_window = false;
    bool show_log = false;
    bool show_file_system_window = true;
    bool show_frametime = false;

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
            show_demo_window = !show_demo_window;
        }

        if (ImGui::IsKeyReleased(ImGuiKey_F2)) {
            show_log = !show_log;
        }

        if (ImGui::IsKeyReleased(ImGuiKey_F3)) {
            show_frametime = !show_frametime;
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

            // Handle if we have no root dir, ask the user to provide a music folder to have as root
            if (app_state.cur_root_dir.empty() && app_state.new_root_dir.empty()) {
                if (tinyfd_messageBox("No music folder found",
                                      "Select 'yes' to choose music folder\nSelect 'no' to close program", "yesno",
                                      "question", 0)) {
                    const char *path = tinyfd_selectFolderDialog("Please Select Music Folder",
                                                                 app_state.cur_root_dir.c_str());
                    if (path) {
                        app_state.new_root_dir = std::string(path);
                    }
                } else {
                    fprintf(stderr, "User selected not to choose music folder!\n");
                    return 0;
                }
            }

            if (ImGui::BeginTable("file_system", 2, table_flags)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                if (app_state.new_root_dir != app_state.cur_root_dir) {
                    scan_folders(app_state.new_root_dir, file_system_cache);
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                render_node(0, file_system_cache);

                ImGui::EndTable();
            }
            ImGui::End();
        }

        /* *
         * If the current selected folder is NOT the same as the current media folder, then we need to rebuild the
         * media player!
        * */
        if (app_state.cur_selected_folder != app_state.cur_media_folder) {
            // Clear the old tracks before we rebuild the list!
            app_state.media_view_tracks.clear();
            build_media_view(app_state.cur_selected_folder);
            debug_log.AddLog("[INFO]: %lu tracks in the list\n", app_state.media_view_tracks.size());
        }
        if (app_state.show_media_view) {
            show_media_view(app_state.cur_selected_folder);
        }

        // * Simple log console copied from the examples in /include/imgui_demo.cpp
        if (show_log) {
            ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
            debug_log.Draw("Debug Log", &show_log);
        }

        if (show_frametime) {
            ImGui::Begin("frametime", &show_frametime);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }
        if (app_state.show_media_view) {
            draw_player_controls();
        }
        // This is where we render all of our draw calls we generated above with the widgets
        ImGui::Render();

        // !! Tracking time test remove later
        Sint64 bytes = SDL_GetAudioStreamQueued(audio_context.audio_stream);
        if (!audio_context.should_stop && bytes >= 0 && !app_state.just_seeked.exchange(false)) {
            int64_t qd_samples = bytes / audio_context.bytes_per_frame;

            int64_t written =
                    audio_context.played_samples.load(std::memory_order_acquire);

            int64_t played = written - qd_samples;
            if (played < 0) {
                played = 0;
                fprintf(stderr, "PLAYED WAS 0\n");
            }

            int64_t cur_seconds =
                    played / audio_context.codec_context->sample_rate;
            if (!app_state.seek_queued && !app_state.is_seeking) {
                app_state.seek_time = cur_seconds;
            } else {
                app_state.seek_queued = false;
            }

            if (cur_seconds >= app_state.cur_selected_track.duration.count() && (app_state.cur_track_index+1 < app_state.media_view_tracks.size())) {
                app_state.cur_track_index++;
                app_state.cur_selected_track = app_state.media_view_tracks[app_state.cur_track_index];
                load_and_play_file(app_state.cur_selected_track);
                debug_log.AddLog("[INFO]: Autoplaying %s", app_state.cur_selected_track.track_name.c_str());
            }
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
    // Shutdown and cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    if (decoder_thread.joinable()) {
        audio_context.should_stop = true;
        decoder_thread.join();
    }
    cleanup_audio(audio_context);
    SDL_Quit();

    return 0;
}
