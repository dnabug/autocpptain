#include "download.hpp"

#include <unistd.h>
#include <ctime>

namespace autocaptain
{

size_t write_function(void* ptr, size_t size, size_t nmemb, std::string* string)
{
    char* buffer = (char*) malloc(size * nmemb + 1);
    memset(buffer, 0, size * nmemb + 1);
    memcpy(buffer, ptr, size * nmemb);

    string->append(std::string(buffer));

    free(buffer);
    return size * nmemb;
}

CURLcode download_init()
{
    return curl_global_init(CURL_GLOBAL_ALL);
}

std::string download_url(std::string url)
{
    CURL* curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);

    std::string result;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    return result;
}

void download_cleanup()
{
    curl_global_cleanup();
}

}
