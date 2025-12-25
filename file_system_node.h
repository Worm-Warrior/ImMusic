//
// Created by harry on 12/24/25.
//

#ifndef IMMUSIC_FILE_SYSTEM_NODE_H
#define IMMUSIC_FILE_SYSTEM_NODE_H
#include <string>

struct file_system_node_t {
    std::string file_name;
    std::string file_type;
    std::string file_path;
};

#endif //IMMUSIC_FILE_SYSTEM_NODE_H