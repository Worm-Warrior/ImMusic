#include "network.h"
#include "../external/simdjson.h"
#include <curl/curl.h>
#include <mutex>

void network_worker(fetch_system& net) {
    fprintf(stderr, "network_worker started\n");
    while (net.running) {
        fetch_request rq;
        {
            std::unique_lock lock(net.req_mutex);
            net.req_cv.wait(lock, [&net]{return !net.req_q.empty() || !net.running;});
            fprintf(stderr, "network_worker woke up\n");
            if (!net.running) break;
            rq = net.req_q.front();
            net.req_q.pop();
        }

        fprintf(stderr, "network_worker dispatching\n");
        switch(rq.type) {
            case ARTISTS:
                fetch_all_artists(rq, net);
                break;
            case ARTIST_ALBUMS:
                fetch_artists_albums(rq, net);
                break;
            case SONGS:
                fetch_album_songs(rq, net);
                break;
            case SETTINGS:
                test_server_settings(rq, net);
                break;
            default:
                fprintf(stderr, "UNREACHABLE CASE IN network_worker EXPLODING\n");
                exit(1);
        }
    }
}

void fetch_all_artists(fetch_request req, fetch_system& net) {
    fetch_result f_res;

    network_response res = {0};

    CURL *curl = curl_easy_init();

    if (!curl) {
        fprintf(stderr, "CURL FAILED\n");
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        exit(1);
    }

    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);

    // data has everything we need now, can cleanup the curl stuff.
    free(res.response);
    curl_easy_cleanup(curl);

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

            f_res.artists.push_back(node);
            printf("[INFO]: Added %s to the artist list\n", node.artist_name.c_str());
        }
    }

    printf("[INFO]: Added %lu artists\n", f_res.artists.size());

    f_res.req = std::move(req);

    {
        std::unique_lock lock(net.res_mutex);
        net.res_q.push(std::move(f_res));
    }
}

void fetch_artists_albums(fetch_request req, fetch_system& net) {
    fetch_result f_res;

   network_response res = {0};

    CURL *curl = curl_easy_init();

    if (!curl) {
        fprintf(stderr, "curl failed easy_init\n");
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        exit(1);
    }
    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);

    // data has everything we need, free curl stuff before parsing
    free(res.response);
    curl_easy_cleanup(curl);

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

        f_res.albums.push_back(a);
    }

    f_res.req = std::move(req);

    {
        std::unique_lock lock(net.res_mutex);
        net.res_q.push(std::move(f_res));
    }
}

void fetch_album_songs(fetch_request req, fetch_system& net) {
    fetch_result f_res;

    network_response res = {0};
    CURL *curl = curl_easy_init();

    if (!curl) {
        fprintf(stderr, "curl failed easy_init\n");
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        exit(1);
    }
    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);

    // Forgot to do this even at the end of the function, whoops.
    free(res.response);
    curl_easy_cleanup(curl);

    simdjson::ondemand::document doc = json_parser.iterate(data);
    simdjson::ondemand::object obj = doc.get_object();

    simdjson::ondemand::array song_array = obj["subsonic-response"]["album"]["song"].get_array();

    for (auto s: song_array) {
        std::cout << s.raw_json() << "\n";
        track_t t;
        t.album_name = std::string(s["album"].get_string().value());
        t.artist_name = std::string(s["artist"].get_string().value());
        t.track_name = std::string(s["title"].get_string().value());


        if (s["track"].error()) {
            t.track_number = 0;
        } else {
            t.track_number = s["track"].get_uint64();
        }

        if (s["duration"].error()) {
            continue;
        }

        t.duration = std::chrono::seconds(s["duration"].get_uint64().value());
        t.song_id = std::string(s["id"].get_string().value());
        f_res.tracks.push_back(t);
    }

    f_res.req = std::move(req);

    {
        std::unique_lock lock(net.res_mutex);
        net.res_q.push(std::move(f_res));
    }
}

void test_server_settings(fetch_request req, fetch_system& net) {
    fetch_result f_res;
    f_res.code = NOT_SET;
    f_res.req = std::move(req);

    auto push_result = [&]() {
        std::unique_lock lock(net.res_mutex);
        net.res_q.push(std::move(f_res));
    };

    network_response res = {0};
    CURL *curl = curl_easy_init();

    if (!curl) {
        fprintf(stderr, "curl failed easy_init\n");
        f_res.code = CURL_FAILURE;
        push_result();
        return;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, network_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&res);
    curl_easy_setopt(curl, CURLOPT_URL, f_res.req.url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        fprintf(stderr, "CURLE WAS NOT OK\n");
        free(res.response);
        curl_easy_cleanup(curl);
        f_res.code = CURL_FAILURE;
        push_result();
        return;
    }

    simdjson::ondemand::parser json_parser;
    simdjson::padded_string data(res.response, res.size);

    free(res.response);
    curl_easy_cleanup(curl);

    simdjson::ondemand::document doc = json_parser.iterate(data);
    simdjson::ondemand::object obj = doc.get_object();
    if (obj.find_field("subsonic-response").error() == simdjson::error_code::NO_SUCH_FIELD) {
        f_res.code = NO_RESPONSE;
        push_result();
        return;
    }

    std::string_view status = obj["subsonic-response"]["status"].get_string();
    if (status == "ok") {
        f_res.code = OK;
    } else {
        uint64_t code = obj["subsonic-response"]["error"]["code"].get_int64();

        if (code == 40) {
            f_res.code = INVALID_LOGIN_CRED;
        }
    }
    push_result();
}
