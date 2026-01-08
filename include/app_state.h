//
// Created by harry on 12/24/25.
//

#ifndef IMMUSIC_APP_STATE_H
#define IMMUSIC_APP_STATE_H
#include <atomic>
#include <string>
#include <filesystem>
#include <vector>

#include "file_tree.h"
#include "track.h"

// * big -> small fields
struct app_state_t {
    std::vector<track_t> media_view_tracks;
    std::vector<track_t> playing_tracks;
    std::vector<file_tree_node_t> file_system_tree;

    std::filesystem::path cur_selected_folder;
    std::filesystem::path cur_media_folder;
    track_t cur_selected_track;

    std::filesystem::path cur_root_dir;
    std::filesystem::path new_root_dir;

    std::string server_url = "http://192.168.4.165:4533/rest/getArtists.view?u=admin&p=rat&c=ImMusic&v=1.16.1&f=json";
    std::string server_base_addr = "http://192.168.4.165:4535";
    std::string server_username = "admin";
    std::string server_password = "rat";

    float cur_track_volume = 0.5;

    int seek_time;
    int seek_max;
    int seek_min = 0;
    uint32_t cur_track_index;

    std::atomic<bool> just_seeked = false;

    bool show_file_system_window = true;
    bool show_media_view = false;

    bool is_running = true;
    bool is_remote = false;
    bool show_player_control = false;
    bool is_seeking = false;
    bool seek_queued = false;

    bool show_remote_browser = false;
    bool show_remote_media_view = false;
    bool show_frametime = false;
};
#endif //IMMUSIC_APP_STATE_H
