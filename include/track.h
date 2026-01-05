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
    std::chrono::seconds duration;

    // Not needed (for now)

    // // If they have the same filepath in the system, then they are the same.
    // bool operator==(const track_t &rhs) const {
    //     return (path == rhs.path);
    // }
    //
    // // Also need this one too.
    // bool operator!=(const track_t &rhs) const {
    //     return !(*this == rhs);
    // }
};
#endif //IMMUSIC_TRACK_H
