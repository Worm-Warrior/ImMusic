//
// Created by harry on 2/19/26.
//

#ifndef IMMUSIC_NETWORK_FETCH_H
#define IMMUSIC_NETWORK_FETCH_H
#include <format>
#include <string>
#include <vector>
#include <curl/curl.h>
#include "network.h"
#include "../external/simdjson.h"
#include "app_state.h"

void rebuild_remote_browser(std::string url) {
    std::string url = std::format("{}/rest/getArtists.view?u={}&p={}&c=ImMusic&v=1.16.1&f=json",
                                  app_state.server_base_addr, app_state.server_username, app_state.server_password);
    network_response res = {0};

    CURL *curl = curl_easy_init();

    if (!curl) {
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        exit(1);
    }

    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);
    simdjson::ondemand::document doc = json_parser.iterate(data);
    simdjson::ondemand::object obj = doc.get_object();

    simdjson::ondemand::array index_array = obj["subsonic-response"]["artists"]["index"].get_array();

    for (auto artist_array: index_array) {
        simdjson::ondemand::array artist = artist_array["artist"].get_array();

        for (auto artist_obj: artist) {
            std::string_view name = artist_obj["name"].get_string();
            std::string_view id = artist_obj["id"].get_string();
            int64_t album_count = artist_obj["albumCount"].get_int64();

            artist_node node;
            node.artist_name = std::string(name);
            node.artist_id = std::string(id);
            node.album_count = album_count;

            app_state.artists.push_back(node);
            debug_log.AddLog("[INFO]: Added %s to the artist list\n", node.artist_name.c_str());
        }
    }

    debug_log.AddLog("[INFO]: Added %lu artists\n", app_state.artists.size());
    free(res.response);
    curl_easy_cleanup(curl);
}

#endif //IMMUSIC_NETWORK_FETCH_H