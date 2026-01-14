/*
 * This will be where we store the persistent data, such as the users music folder, etc.
 */

#ifndef IMMUSIC_APP_SETTINGS_H
#include <filesystem>

struct app_settings_t {
    std::filesystem::path default_path;
    std::string base_url;
    std::string username;
    std::string password;
};

#define IMMUSIC_APP_SETTINGS_H

#endif //IMMUSIC_APP_SETTINGS_H
