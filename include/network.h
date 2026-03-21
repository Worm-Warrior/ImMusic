#ifndef IMMUSIC_CURL_RESPONSE_H
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <string>
#include <cstdlib>
#include <cstring>

enum FETCH_STATUS {
    IDLE,
    LOADING,
    READY,
};

enum REQ_TYPE {
    ARTISTS,
    ARTIST_ALBUMS,
    SONGS,
};

enum VALIDATION_CODE {
    OK = 0,
    NO_RESPONSE,
    SUBSONIC_NOT_OK,
    CURL_FAILURE,
    INVALID_LOGIN_CRED,
    WAITING_FOR_RESPONSE,
};

struct album_node {
    FETCH_STATUS status = IDLE;
    std::string album_name;
    std::string album_id;
    uint32_t track_count;
};

struct artist_node {
    FETCH_STATUS status = IDLE;
    std::vector<album_node> albums;
    std::string artist_name;
    std::string artist_id;
    uint32_t album_count;
};

/*
 * The following code was taken from the libcurl examples page, code by Daniel Stenberg:
 * https://curl.se/libcurl/c/getinmemory.html
 */

struct network_response {
    char *response;
    size_t size;
};

static size_t network_callback(char *data, size_t size, size_t nmemb, void *clientp) {
    size_t realsize = nmemb;
    struct network_response *mem = (struct network_response *) clientp;

    char *ptr = (char *) realloc(mem->response, mem->size + realsize + 1);
    if (!ptr)
        return 0; /* out of memory */

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

// My networking garbo


struct fetch_request {
    REQ_TYPE type;
    std::string artist_id;
    std::string album_id;
};

struct fetch_system {
    std::mutex req_mutex;
    std::queue<fetch_request> req_q;

    std::mutex res_mutex;
    std::queue<fetch_request> res_q;

    std::atomic<bool> running{true};
    std::thread worker;
};

extern fetch_system net;

void fetch_all_artists(std::string url);

void fetch_artists_albums(artist_node &artist);

void fetch_album_songs(std::string album_id);



#define IMMUSIC_CURL_RESPONSE_H

#endif //IMMUSIC_CURL_RESPONSE_H
