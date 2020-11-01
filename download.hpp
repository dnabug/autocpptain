#pragma once

#include <curl/curl.h>
#include <string>

namespace autocaptain
{

CURLcode download_init();
std::string download_url(std::string url);
void download_cleanup();

}
