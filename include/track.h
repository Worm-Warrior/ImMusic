//
// Created by harry on 12/26/25.
//

#ifndef IMMUSIC_TRACK_H
#define IMMUSIC_TRACK_H
#include <filesystem>

struct track_t {
    // ? maybe we make a remote and local track struct instead of a bool for this program ?
    bool is_remote;

    std::filesystem::path path;
    std::string track_name;
    std::string album_name;
    int track_number;
    int duration; // Duration in seconds?
};
#endif //IMMUSIC_TRACK_H