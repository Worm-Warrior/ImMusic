#ifndef IMMUSIC_CURL_RESPONSE_H
#include <cstdlib>
#include <cstring>

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


#define IMMUSIC_CURL_RESPONSE_H

#endif //IMMUSIC_CURL_RESPONSE_H
