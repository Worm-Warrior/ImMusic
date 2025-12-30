//
// Created by harry on 12/24/25.
//

#ifndef IMMUSIC_APP_STATE_H
#define IMMUSIC_APP_STATE_H
#include <string>
#include <filesystem>
#include <vector>

#include "file_tree.h"
#include "track.h"

// * big -> small fields
struct app_state_t {
    std::vector<track_t> media_view_tracks;
    std::vector<file_tree_node_t> file_system_tree;

    std::filesystem::path cur_selected_folder;
    std::filesystem::path cur_media_folder;
    std::filesystem::path cur_selected_track;

    std::filesystem::path cur_root_dir;
    std::filesystem::path new_root_dir;

    bool show_media_view = true;
    bool is_running = true;
    bool is_remote = false;
};
#endif //IMMUSIC_APP_STATE_H
