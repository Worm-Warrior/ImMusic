//
// Created by harry on 12/26/25.
//

#ifndef IMMUSIC_TRACK_H
#define IMMUSIC_TRACK_H
#include <filesystem>

// * big -> small fields
struct track_t {
    std::filesystem::path path;
    std::string track_name;
    std::string artist_name;
    std::string album_name;
    // We don't need signed integers for these
    unsigned int track_number;
    unsigned int duration; // Duration in seconds?

    bool is_remote;
};
#endif //IMMUSIC_TRACK_H
