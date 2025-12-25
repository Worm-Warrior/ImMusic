//
// Created by harry on 12/24/25.
//

#ifndef IMMUSIC_APP_STATE_H
#define IMMUSIC_APP_STATE_H
#include <string>

struct app_state_t {
    bool is_running = true;
    bool is_remote = false;
    std::string root_dir;
    std::string current_folder;
};
#endif //IMMUSIC_APP_STATE_H