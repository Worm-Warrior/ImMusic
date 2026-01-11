//
// Created by harry on 12/26/25.
//

#ifndef IMMUSIC_TRACK_H
#define IMMUSIC_TRACK_H
#include <filesystem>
#include <variant>

struct remote_id_t {
    // The remote id can be an array of bytes, not a string.
    std::array<uint8_t, 16> bytes;
};

using track_location = std::variant<std::filesystem::path, remote_id_t>;

// * big -> small fields
struct track_t {
    std::filesystem::path path;

    track_location location;

    std::string track_name;
    std::string artist_name;
    std::string album_name;
    // We don't need signed integers for these
    uint32_t track_number;
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
