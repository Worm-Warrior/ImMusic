/*
 * This is for caching the remote view, we want to send as few network requests as we can, and try to lazy load
 * when possible.
 */


#ifndef IMMUSIC_REMOTE_TREE_H
#include <cstdint>
#include <string>
#include <vector>


struct album_node {
    std::string album_name;
    std::string album_id;
    uint32_t track_count;
};

struct artist_node {
    std::vector<album_node> albums;
    std::string artist_name;
    std::string artist_id;
    uint32_t album_count;
};

#define IMMUSIC_REMOTE_TREE_H

#endif //IMMUSIC_REMOTE_TREE_H
