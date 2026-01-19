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
#include <fstream>
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
#include "include/file_tree.h"
#include "external/tinyfiledialogs.h"
#include "include/network.h"
#include <curl/curl.h>
#include "external/simdjson.h"

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

            ImGui::MenuItem("Server Settings", NULL, &app_state.show_server_settings);


            if (ImGui::MenuItem("Search")) {
            }
            if (ImGui::MenuItem("Local File System View", NULL, &app_state.show_file_system_window)) {
                if (app_state.show_remote_browser) {
                    app_state.show_remote_browser = !app_state.show_remote_browser;
                }
            }
            if (ImGui::MenuItem("Remote View", NULL, &app_state.show_remote_browser)) {
                if (app_state.show_file_system_window) {
                    app_state.show_file_system_window = !app_state.show_file_system_window;
                }
            }
            ImGui::Separator();
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

void draw_node(int node_idx, const file_system_cache_t &cache) {
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
            draw_node(i, cache);
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

// TODO: This currently has big overhead because we stop and start a thread each playback.
void load_and_play_file(const track_t &track) {
    if (decoder_thread.joinable()) {
        audio_context.should_stop = true;
        decoder_thread.join();
    }

    std::string track_path;

    if (!track.path.empty()) {
        track_path = track.path;
    } else {
        track_path = std::format(
            "{}/rest/stream?id={}&u={}&p={}&c=ImMusic&v=1.16.1&f=json", app_state.server_base_addr, track.song_id,
            app_state.server_username, app_state.server_password);
    }

    if (!load_file(audio_context, track_path)) {
        debug_log.AddLog("[ERROR]: Failed to load file %s\n", track_path.c_str());
        return;
    }
    debug_log.AddLog("[INFO]: Loaded file: %s\n", track_path.c_str());

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
    debug_log.AddLog("[INFO]: Now playing: %s\n", track_path.c_str());
}

void draw_file_system_window() {
    ImGui::Begin("Local File System", &app_state.show_file_system_window);
    static ImGuiTableFlags table_flags =
            ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

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
            exit(2);
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
        draw_node(0, file_system_cache);

        ImGui::EndTable();
    }
    ImGui::End();
}

/* *
 * === MEDIA VIEW ===
 * This will build up our media view.
 *
 * Ideas for later:
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

void draw_media_view(std::filesystem::path path) {
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
    ImGui::LabelText("", "%s | %s | %s", app_state.cur_selected_track.track_name.c_str(),
                     app_state.cur_selected_track.artist_name.c_str(), app_state.cur_selected_track.album_name.c_str());
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

    ImGui::SameLine();
    if (ImGui::Checkbox("Autoplay Loop", &app_state.should_repeat_autoplay)) {
    }

    if (ImGui::SliderInt("Time", &app_state.seek_time, app_state.seek_min, app_state.seek_max)) {
        app_state.is_seeking = true;
    } else if (ImGui::IsItemDeactivated() && app_state.is_seeking != false) {
        debug_log.AddLog("Seeking released at: %d\n", app_state.seek_time);
        app_state.is_seeking = false;
        app_state.seek_queued = true;

        // * This is here because if we don't delay by a frame we can get garbage, so delay a frame.
        // ! This does not work and still looks bad, idk really what to do about it !
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


// * === FRAMETIME TEXT ===
void draw_frametime() {
    ImGuiIO &io = ImGui::GetIO();
    ImGui::Begin("frametime", &app_state.show_frametime);

    float avg_ms = 1000.0f / io.Framerate;
    ImGui::Text("Average %.3f ms/frame (%.1f FPS)", avg_ms, io.Framerate);

    ImGui::End();
}

// * === REMOTE BROWSER ===

// This function uses network_response and network_callback defined in network.h
// TODO: Make multi threaded!
// !!! NOT ASYNC YET !!!
void rebuild_remote_browser() {
    std::string url = std::format("{}/rest/getArtists.view?u={}&p={}&c=ImMusic&v=1.16.1&f=json",
                                  app_state.server_base_addr, app_state.server_username, app_state.server_password);
    network_response res = {0};

    CURL *curl = curl_easy_init();

    if (!curl) {
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        exit(1);
    }

    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);
    simdjson::ondemand::document doc = json_parser.iterate(data);
    simdjson::ondemand::object obj = doc.get_object();

    simdjson::ondemand::array index_array = obj["subsonic-response"]["artists"]["index"].get_array();

    for (auto artist_array: index_array) {
        simdjson::ondemand::array artist = artist_array["artist"].get_array();

        for (auto artist_obj: artist) {
            std::string_view name = artist_obj["name"].get_string();
            std::string_view id = artist_obj["id"].get_string();
            int64_t album_count = artist_obj["albumCount"].get_int64();

            artist_node node;
            node.artist_name = std::string(name);
            node.artist_id = std::string(id);
            node.album_count = album_count;

            app_state.artists.push_back(node);
            debug_log.AddLog("[INFO]: Added %s to the artist list\n", node.artist_name.c_str());
        }
    }

    debug_log.AddLog("[INFO]: Added %lu artists\n", app_state.artists.size());
    free(res.response);
    curl_easy_cleanup(curl);
}

// TODO: Make multi threaded!
// !!! NOT ASYNC YET !!!
void fetch_artist_albums(artist_node &artist) {
    std::string url = std::format(
        "{}/rest/getArtist.view?id={}&u={}&p={}&c=ImMusic&v=1.16.1&f=json",
        app_state.server_base_addr, artist.artist_id, app_state.server_username, app_state.server_password);
    network_response res = {0};

    CURL *curl = curl_easy_init();

    if (!curl) {
        fprintf(stderr, "curl failed easy_init\n");
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        exit(1);
    }
    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);
    simdjson::ondemand::document doc = json_parser.iterate(data);
    simdjson::ondemand::object obj = doc.get_object();

    simdjson::ondemand::array album_array = obj["subsonic-response"]["artist"]["album"].get_array();

    for (auto album: album_array) {
        album_node a;
        std::string_view id = album["id"].get_string();
        std::string_view name = album["name"].get_string();
        a.album_id = std::string(id);
        a.album_name = std::string(name);
        a.track_count = album["songCount"].get_int64();

        artist.albums.push_back(a);
    }

    free(res.response);
    curl_easy_cleanup(curl);
}


void draw_remote_tree(std::vector<artist_node> &artists) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_OpenOnArrow;

    for (auto &a: artists) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        bool open = ImGui::TreeNodeEx(a.artist_name.c_str(), flags);

        if (ImGui::IsItemClicked() && a.artist_id != app_state.cur_artist) {
            app_state.cur_artist = a.artist_id;
            debug_log.AddLog("[INFO]: selected artist with id %s\n", a.artist_id.c_str());
        }

        ImGui::TableNextColumn();
        ImGui::Text("%d", a.album_count);
        ImGui::TableNextColumn();
        ImGui::Text("Artist");

        if (open) {
            if (a.albums.empty()) {
                debug_log.AddLog("[INFO]: Fetching albums for %s\n", a.artist_name.c_str());
                fetch_artist_albums(a);
            }

            for (auto album: a.albums) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TreeNodeEx(album.album_name.c_str(),
                                  ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet |
                                  ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAllColumns);
                if (ImGui::IsItemClicked()) {
                    debug_log.AddLog("[INFO]: Selected album %s\n", album.album_name.c_str());
                    app_state.selected_album = album.album_id;
                    app_state.show_remote_media_view = true;
                    app_state.show_media_view = false;
                }
                ImGui::TableNextColumn();
                ImGui::Text("%d", album.track_count);
                ImGui::TableNextColumn();
                ImGui::Text("Album");
            }
            ImGui::TreePop();
        }
    }
}

void draw_remote_browser() {
    if (app_state.should_rebuild_remote_browser) {
        debug_log.AddLog("[INFO]: Rebuilding remote artists list\n");
        debug_log.AddLog("[INFO]: New URL: %s\n", app_state.server_base_addr.c_str());
        rebuild_remote_browser();
        app_state.should_rebuild_remote_browser = false;
    }

    ImGui::Begin("Remote Browser", &app_state.show_remote_browser);

    ImGuiTableFlags table_flags =
            ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

    if (ImGui::BeginTable("remote_browser", 3, table_flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        draw_remote_tree(app_state.artists);
        ImGui::EndTable();
    }
    ImGui::End();
}

// * === REMOTE MEDIA VIEW ===

void display_remote_tracks(const std::vector<track_t> &tracks) {
    ImGuiListClipper clipper;
    clipper.Begin(tracks.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const track_t &t = tracks[i];
            bool is_selected = (app_state.cur_selected_track.song_id == t.song_id);


            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            if ((ImGui::Selectable(("##" + t.song_id).c_str(), is_selected,
                                   ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) &&
                ImGui::IsMouseDoubleClicked(0)) {
                app_state.playing_tracks = app_state.media_view_tracks;
                // Update the state so we have the right index(es).
                app_state.cur_selected_track = t;
                uint32_t index = 0;
                for (const track_t &track: app_state.playing_tracks) {
                    if (track.song_id == t.song_id) {
                        break;
                    }
                    index++;
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

// TODO: Make multi threaded!
// !!! NOT ASYNC YET !!!
void build_remote_media_view(std::string album_id) {
    std::string url = std::format(
        "{}/rest/getAlbum.view?id={}&u={}&p={}&c=ImMusic&v=1.16.1&f=json", app_state.server_base_addr, album_id,
        app_state.server_username, app_state.server_password);

    network_response res = {0};
    CURL *curl = curl_easy_init();

    if (!curl) {
        fprintf(stderr, "curl failed easy_init\n");
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        exit(1);
    }
    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);
    simdjson::ondemand::document doc = json_parser.iterate(data);
    simdjson::ondemand::object obj = doc.get_object();

    simdjson::ondemand::array song_array = obj["subsonic-response"]["album"]["song"].get_array();

    for (auto s: song_array) {
        track_t t;
        t.album_name = std::string(s["album"].get_string().value());
        t.artist_name = std::string(s["artist"].get_string().value());
        t.track_name = std::string(s["title"].get_string().value());

        if (s.find_field("track").error() == simdjson::error_code::NO_SUCH_FIELD) {
            t.track_number = 0;
        } else {
            t.track_number = s["track"].get_uint64();
        }

        if (s.find_field("duration").error() == simdjson::error_code::NO_SUCH_FIELD) {
            continue;
        }

        t.duration = std::chrono::seconds(s["duration"].get_uint64().value());
        t.song_id = std::string(s["id"].get_string().value());
        app_state.media_view_tracks.push_back(t);
    }
}

void draw_remote_media_view() {
    if (app_state.cur_album != app_state.selected_album) {
        app_state.cur_album = app_state.selected_album;
        app_state.media_view_tracks.clear();
        build_remote_media_view(app_state.cur_album);
        debug_log.AddLog("[INFO]: Rebuilding remote media view\n");
    }
    ImGui::Begin("Remote Media View", &app_state.show_remote_media_view);
    ImGuiTableFlags media_table_flags =
            ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Hideable |
            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("media_view_remote", 5, media_table_flags)) {
        ImGui::TableSetupColumn(
            "Track",
            ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortAscending |
            ImGuiTableColumnFlags_DefaultSort, 80.0f);
        ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Artist", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Album", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        display_remote_tracks(app_state.media_view_tracks);
        ImGui::EndTable();
    }
    ImGui::End();
}


// * === SETTINGS MANAGEMENT ===

void save_settings_to_file() {
    std::ofstream config("settings.txt");

    if (!config.is_open()) {
        fprintf(stderr, "could not open file to save settings!\n");
    }

    config << "music_path=" << app_state.cur_root_dir.string() << "\n";
    config << "server_url=" << app_state.server_base_addr << "\n";
    config << "username=" << app_state.server_username << "\n";
    config << "password=" << app_state.server_password << "\n";
}

bool parse_settings(std::fstream &config) {
    int line_number = 1;
    std::string line;
    while (std::getline(config, line)) {
        if (line.starts_with('#')) {
            continue;
        }

        if (line.starts_with("music_path")) {
            size_t split = line.find_first_of('=');

            std::string dir = line.substr(split + 1, line.size());
            std::cout << dir << "\n";
            if (std::filesystem::exists(dir)) {
                app_state.new_root_dir = std::filesystem::path(dir);
            } else {
                fprintf(stderr, "ERROR AT LINE %d: FILEPATH music_path=%s IN settings.txt DOES NOT EXIST!\n",
                        line_number, dir.c_str());
                return false;
            }
        }

        if (line.starts_with("server_url")) {
            size_t split = line.find_first_of('=');
            std::string base_url = line.substr(split + 1, line.size());
            app_state.server_base_addr = base_url;
            std::cout << base_url << "\n";
        }

        if (line.starts_with("username")) {
            size_t split = line.find_first_of('=');
            std::string username = line.substr(split + 1, line.size());
            app_state.server_username = username;
            std::cout << username << "\n";
        }

        if (line.starts_with("password")) {
            size_t split = line.find_first_of('=');
            std::string password = line.substr(split + 1, line.size());
            app_state.server_password = password;
            std::cout << password << "\n";
        }
        line_number++;
    }
    return true;
}

void check_settings_file() {
    std::fstream config;
    config.open("settings.txt");
    if (config.is_open()) {
        if (!parse_settings(config)) {
            fprintf(stderr, "parse_settings returned false!\n");
        }
    }
}

// TODO: Make multi threaded!
// !!! NOT ASYNC YET !!!
VALIDATION_CODE validate_server_info(const std::string &addr, const std::string &username,
                                     const std::string &password) {
    std::string url = std::format("{}/rest/ping?u={}&p={}&c=ImMusic&v=1.16.1&f=json", addr, username, password);

    network_response res = {0};
    CURL *curl = curl_easy_init();

    if (!curl) {
        fprintf(stderr, "curl failed easy_init\n");
        return CURL_FAILURE;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        fprintf(stderr, "CURLE WAS NOT OK\n");
        return CURL_FAILURE;
    }

    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);
    simdjson::ondemand::document doc = json_parser.iterate(data);
    simdjson::ondemand::object obj = doc.get_object();
    if (obj.find_field("subsonic-response").error() == simdjson::error_code::NO_SUCH_FIELD) {
        return NO_RESPONSE;
    }

    std::string_view status = obj["subsonic-response"]["status"].get_string();
    if (status == "ok") {
        return OK;
    }

    uint64_t code = obj["subsonic-response"]["error"]["code"].get_int64();

    if (code == 40) {
        return INVALID_LOGIN_CRED;
    }

    free(res.response);
    curl_easy_cleanup(curl);
}

void draw_settings_menu() {
    ImGui::Begin("Server Settings", &app_state.show_server_settings);

    static char urlbuf[256] = "";
    static char username[32] = "";
    static char password[32] = "";
    static VALIDATION_CODE last_error = OK;


    ImGui::TextWrapped("CAUTION:\nAll of this info is saved in plain text in settings.txt!");
    ImGui::InputTextWithHint("Server url", "ex: http://192.168.4.165:4535", urlbuf, IM_COUNTOF(urlbuf));
    ImGui::InputText("Username", username, IM_COUNTOF(username));
    ImGui::InputTextWithHint("Password", "THIS IS STORED IN PLAIN TEXT", password, IM_COUNTOF(password),
                             ImGuiInputTextFlags_Password);

    // We should do a sanity check if the info works with a quick test curl request!
    if (ImGui::Button("Save")) {
        VALIDATION_CODE code = validate_server_info(std::string(urlbuf), std::string(username), std::string(password));

        if (code == OK) {
            app_state.server_base_addr = std::string(urlbuf);
            app_state.server_username = std::string(username);
            app_state.server_password = std::string(password);
            app_state.show_server_settings = false;
        } else {
            last_error = code;
            ImGui::OpenPopup("ERROR!");
        }
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("ERROR!", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string error_text = "";
        switch (last_error) {
            case CURL_FAILURE:
                error_text = "CURL has failed, your URL is most likely invalid.";
                break;
            case NO_RESPONSE:
                error_text = "There was no subsonic response, your URL most likely not a navidrome server.";
                break;
            case SUBSONIC_NOT_OK:
                error_text = "Subsonic returned a status failed on your request.";
                break;
            case INVALID_LOGIN_CRED:
                error_text = "Your username or password was invalid for the server provided.";
                break;
            default: ;
        }

        ImGui::Text("%s", error_text.c_str());
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }
    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
        memset(urlbuf, 0, sizeof(urlbuf));
        memset(username, 0, sizeof(username));
        memset(password, 0, sizeof(password));
        app_state.show_server_settings = false;
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
    SDL_GL_SetSwapInterval(-1); // This is vsync
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

    check_settings_file();

    // Make the bg invisible
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0, 0, 0, 0);

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool show_demo_window = false;
    bool show_log = false;
    app_state.show_frametime = false;

    bool window_focused = true;

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

            if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                window_focused = true;
            }

            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                window_focused = false;
            }
        }

        // !!! This does not work on wayland, find a way to limit framerate on minimized/lose focus !!!
        // So don't do anything on a minimized window!
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        if (!window_focused) {
            SDL_Delay(33);
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
            app_state.show_frametime = !app_state.show_frametime;
        }

        // * This is the file browser window stuff
        if (app_state.show_file_system_window && !app_state.show_remote_browser) {
            draw_file_system_window();
        }

        // * Remote Browser
        if (app_state.show_remote_browser && !app_state.show_file_system_window) {
            draw_remote_browser();
        }

        // * Remote Media View
        if (app_state.show_remote_media_view && !app_state.show_media_view) {
            draw_remote_media_view();
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

        // * Show local media view
        if (app_state.show_media_view && !app_state.show_remote_browser) {
            draw_media_view(app_state.cur_selected_folder);
        }

        // * Simple log console copied from the examples in /include/imgui_demo.cpp
        if (show_log) {
            ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
            debug_log.Draw("Debug Log", &show_log);
        }

        // * Show frametime window
        if (app_state.show_frametime) {
            draw_frametime();
        }

        // * Show player controls
        if (app_state.show_media_view || app_state.show_remote_media_view) {
            draw_player_controls();
        }

        // * For changing the server settings
        if (app_state.show_server_settings) {
            draw_settings_menu();
        }

        // This is where we render all of our draw calls we generated above with the widgets
        ImGui::Render();

        // * === PROGRESS BAR SYNC ===
        // TODO: Refactor out into function?
        // Right now this is what keeps track of the current playback time.
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

            if (cur_seconds >= app_state.cur_selected_track.duration.count() && (
                    app_state.cur_track_index + 1 < app_state.playing_tracks.size())) {
                app_state.cur_track_index++;
                app_state.cur_selected_track = app_state.playing_tracks[app_state.cur_track_index];
                load_and_play_file(app_state.cur_selected_track);
                debug_log.AddLog("[INFO]: Autoplaying %s", app_state.cur_selected_track.track_name.c_str());
            } else if (cur_seconds >= app_state.cur_selected_track.duration.count() && app_state.should_repeat_autoplay) {
                app_state.cur_track_index = 0;
                app_state.cur_selected_track = app_state.playing_tracks[0];
                load_and_play_file(app_state.cur_selected_track);
                debug_log.AddLog("[INFO]: Autoplaying+Looping %s", app_state.cur_selected_track.track_name.c_str());
            }
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
    save_settings_to_file();
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
