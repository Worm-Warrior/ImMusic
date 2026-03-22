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
    fprintf(stderr, "IMPLEMENT ME\n");
    exit(1);
}

void fetch_album_songs(fetch_request req, fetch_system& net) {
    fprintf(stderr, "IMPLEMENT ME\n");
    exit(1);
}
