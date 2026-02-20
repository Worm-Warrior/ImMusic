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

static std::vector<artist_node> artist_staging;
static std::mutex artist_staging_mutex;
static std::atomic<bool> artists_ready = false;
static std::atomic<bool> rebuild_browser_fetching = false;

void rebuild_remote_browser(std::string url) {
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

    std::vector<artist_node> local;
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

            local.push_back(node);
        }
    }
    free(res.response);
    curl_easy_cleanup(curl);

    {
        std::lock_guard lock(artist_staging_mutex);
        artist_staging = std::move(local);
    }
    artists_ready.store(true);
}

static std::vector<album_node> album_staging;
static std::mutex album_staging_mutex;
static std::atomic<bool> album_ready = false;
static std::atomic<bool> fetch_albums = false;

void fetch_artist_albums(std::string url) {
    network_response res = {0};

    CURL *curl = curl_easy_init();

    if (!curl) {
        fprintf(stderr, "curl failed easy_init\n");
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        exit(1);
    }
    std::vector<album_node> local;
    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);
    simdjson::ondemand::document doc = json_parser.iterate(data);
    simdjson::ondemand::object obj = doc.get_object();

    simdjson::ondemand::array album_array = obj["subsonic-response"]["artist"]["album"].get_array();

    for (auto album: album_array) {
        album_node a;
        std::string_view id = album["id"].get_string();
        std::string_view name = album["name"].get_string();
        a.album_id = std::string(id);
        a.album_name = std::string(name);
        a.track_count = album["songCount"].get_int64();

        local.push_back(a);
    }

    free(res.response);
    curl_easy_cleanup(curl);
    {
        std::lock_guard lock(album_staging_mutex);
        album_staging = std::move(local);
    }
}


#endif //IMMUSIC_NETWORK_FETCH_H