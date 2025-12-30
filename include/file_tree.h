//
// Created by harry on 12/29/25.
//

#ifndef IMMUSIC_FILE_TREE_H
#define IMMUSIC_FILE_TREE_H
#include <filesystem>
#include <vector>

struct file_tree_node_t {
    std::string name;
    std::filesystem::path path;
    int parent_index = -1; // -1 for root.
    std::vector<int> child_indexes;
    bool has_children;
};

struct file_system_cache_t {
    std::vector<file_tree_node_t> nodes;
    std::filesystem::path root;
    bool is_valid;
};

#endif //IMMUSIC_FILE_TREE_H