#include <iostream>
#include <map>

#include "system.hpp"
#include "download.hpp"

#define AC_VERSION "1"

void help(std::map<std::string, std::string> help_str, std::string cmd)
{
    std::cout << "C++ Autocaptain: version " << AC_VERSION << std::endl;
    if (!cmd.empty()) {
        if (!help_str.count(cmd))
            std::cout << "Command `" << cmd << "` not found" << std::endl;
        else std::cout << cmd << ": " << help_str[cmd] << std::endl;
        return;
    }

    for (auto x : help_str) {
        std::cout << x.first << ": " << x.second << std::endl;
    }
}

int main(int argc, char** argv)
{
    using namespace autocaptain;

    std::map<std::string, std::string> help_strings;

    help_strings.emplace("help", "Show the help log for a specific command or for all");
    help_strings.emplace("server", "Start a server.");
    help_strings.emplace("port", "If running a server, specify the port to listen on.");
    help_strings.emplace("config", "Configuration file to use.");
    help_strings.emplace("update", "Download and incorporate missing logs in logs list");

    if (argc < 2) {
        help(help_strings, std::string());
        return 0;
    }

    std::string config = "config.json";
    bool do_update = false;

    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "help") {
            if (i + 1 < argc) {
                help(help_strings, std::string(argv[i + 1]));
            } else help(help_strings, std::string());
            return 0;
        } else if (std::string(argv[i]) == "update") {
            do_update = true;
        } else if (std::string(argv[i]) == "config") {
             if (i + 1 < argc) {
                config = std::string(argv[i + 1]);
                i++;
            } else {
                std::cout << "No file specified for config" << std::endl;
                return 1;
            }
        }
    }

    download_init();
    System system(config, do_update);

    download_cleanup();
    return 0;
}
