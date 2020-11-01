#include "system.hpp"

#include "download.hpp"
#include "util.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

#include <unistd.h>

namespace autocaptain
{

using nlohmann::json;

System::System(std::string config_path, bool do_update)
{
    std::ifstream config(config_path, std::ios::binary);
    if (!config.good()) {
        std::cerr << "Failed to open config file " << config_path << std::endl;
        return;
    }

    json config_json;
    config >> config_json;
    if (!config_json.count("logs-list") || !config_json.count("saved-data") ||
        !config_json.count("name") || !config_json.count("logs-path") ||
        !config_json.count("aliases")) {
        std::cerr << "One or more fields missing in " << config_path << "." << std::endl;
        return;
    }

    std::string data_file = config_json["saved-data"],
                logs_file = config_json["logs-list"],
                logs_path = config_json["logs-path"],
                name = config_json["name"],
                aliases_file = config_json["aliases"];

    std::ifstream data(data_file, std::ios::binary);

    if (!data.is_open()) {
        // We have no data file, so we must download logs or exit.
        std::cerr << "Failed to open data file " << data_file << std::endl;
    } else if (!do_update) { // No request to get latest games.
        json data_json;
        data >> data_json;
        data_json["logs"].get_to(logs_list);
        data_json["name"].get_to(name);

        data.close();
        return;
    }

    std::ifstream logs(logs_file, std::ios::binary);

    if (!logs.is_open()) {
        std::cerr << "Failed to open log file " << logs_file << std::endl;
        std::cerr << "No logs/data, closing" << std::endl;
        exit(1);
    }

    // No saved data file but we have a logs list.

    const std::streamsize len = 64;
    char* line = (char*) malloc(len);
    memset(line, 0, len);

    while (logs.getline(line, len)) {
        std::string str(line);
        if (str.empty()) continue;

        try {
            std::uint32_t id = std::stoul(str);
            bool dup = false;
            for (std::uint32_t i : logs_list)
                if (i == id) { dup = true; break; }

            if (!dup) logs_list.push_back(std::stoul(str));
        } catch (std::invalid_argument& e) {}
    }

    std::cout << "Found " << logs_list.size() << " logs in " << logs_file << std::endl;
    std::cout << "System: " << name << std::endl;

    DownloadLogs(logs_path);

    std::cout << "Found " << players.size() << " players" << std::endl;

    FetchAliases(aliases_file);
    WriteData(data_file, aliases_file);
}

void System::DownloadLogs(std::string logs_path)
{
    int dl_counter = 0;

    for (std::uint32_t i : logs_list) {
        std::string file_path = logs_path + "/" + std::to_string(i);
        std::ifstream file(file_path, std::ios::binary);
        if (file.good()) {
            char c;
            std::vector<std::uint8_t> msg_pack;
            while (!file.get(c).eof()) msg_pack.push_back(c);

            AddLog(json::from_msgpack(msg_pack), i);
            file.close();
            continue;
        }

        std::string url = "https://logs.tf/json/";
        url += std::to_string(i);
        std::string content = download_url(url);
        json log;
        try {
            log = json::parse(content);
            log.erase("chat");
            AddLog(log, i);
        } catch (json::parse_error& e) {
            std::cerr << "Invalid log ID `" << i << "`" << std::endl;
            std::cerr << e.what() << std::endl;
            std::cerr << content << std::endl;
            continue;
        }

        std::ofstream out(file_path, std::ios::binary);
        if (out.good()) {
            for (std::uint8_t i : json::to_msgpack(log)) {
                out << i;
            }
            out.close();
        }

        dl_counter++;

        if (dl_counter % 10 == 0) {
            sleep(3);
        }
    }
}

void System::FetchAliases(std::string aliases_file)
{
    std::ifstream a_file(aliases_file, std::ios::binary);
    json aliases_json;
    if (a_file.good()) {
        a_file >> aliases_json;
        a_file.close();
    }

    int dl_counter = 0;

    for (std::size_t i = 0; i < players.size(); i++) {
        std::string sid3 = players[i].sid3;
        if (aliases_json.count(sid3)) {
            players[i].alias = aliases_json[sid3];
            continue;
        }

        players[i].alias = RetrievePlayerAlias(sid3);
        dl_counter++;

        if (dl_counter % 10 == 0) {
            sleep(3);
        }
    }
}

void System::WriteData(std::string data_file, std::string aliases_file)
{
    std::ofstream a_file(aliases_file, std::ios::binary);

    if (a_file.good()) {
        json j;
        for (Player p : players) {
            if (!p.alias.empty() && p.alias != p.sid3) j[p.sid3] = p.alias;
        }
        a_file << j;
    }
}

Role get_role(json player, json players_list, int gametime, int playtime)
{
    if (playtime < gametime / 3) {
        return None;
    }

    bool combo = false;
    for (auto other : players_list) {
        if (player["team"] != other["team"] ||
            player["class_stats"][0]["type"] != other["class_stats"][0]["type"]) continue;

        unsigned int other_time = 0;
        for (auto x : other["class_stats"]) other_time += x["total_time"].get<int>();

        if (other_time < gametime / 3) continue;

        combo = player["hr"] > other["hr"];
        if (combo) break;
    }

    std::string main_class = player["class_stats"][0]["type"];

    if (main_class == "demoman") return Demoman;
    if (main_class == "medic") return Medic;
    if (main_class == "scout") return combo ? PocketScout : FlankScout;
    if (main_class == "soldier") return combo ? Pocket : Roamer;
    return Offclass;
}

void System::AddLog(json log_json, std::uint32_t id)
{
    Game g;
    // First, construct teams
    g.blue_rounds = log_json["teams"]["Blue"]["score"];
    g.red_rounds = log_json["teams"]["Red"]["score"];
    g.log_id = id;

    std::string winning_team = g.blue_rounds == g.red_rounds ? std::string() :
                               (g.blue_rounds > g.red_rounds ? "Blue" : "Red");

    json player_list = log_json["players"];
    int gametime = log_json["length"].get<int>();

    for (auto player : player_list.items()) {
        std::string player_sid = player.key();
        json plvalue = player.value();

        // Construct players if not present, fill in their stats

        int playtime = 0;
        for (auto x : plvalue["class_stats"]) playtime += x["total_time"].get<int>();

        Player* p;
        if (!(p = GetPlayer(player_sid))) {
            players.push_back(Player { .alias = std::string(),
                                       .sid3 = player_sid });
            p = GetPlayer(player_sid);
        }
        Role role = get_role(plvalue, player_list, gametime, playtime);
        if (plvalue["team"] == "Blue") {
            g.blue_team[role] = player_sid;
        } else {
            g.red_team[role] = player_sid;
        }

        PlayerRoleStats prs = p->stats[role];

        update_average(&prs.dpm, (plvalue["dmg"].get<float>() * 60) / playtime, prs.games);
        update_average(&prs.dtpm, (plvalue["dt"].get<float>() * 60) / playtime, prs.games);
        update_average(&prs.kpm, (plvalue["kills"].get<float>() * 60) / playtime,
                       prs.games);
        update_average(&prs.apm, (plvalue["assists"].get<float>() * 60) / playtime, prs.games);
        update_average(&prs.dthspm, (plvalue["deaths"].get<float>() * 60) / playtime, prs.games);

        prs.games++;
        if (winning_team == plvalue["team"]) prs.wins++;

        p->stats[role] = prs;
    }
}

Player* System::GetPlayer(PlayerID id)
{
    for (std::size_t i = 0; i < players.size(); i++) {
        if (players[i].alias == id || players[i].sid3 == id) return &players[i];
    }

    return nullptr;
}

std::string System::RetrievePlayerAlias(PlayerID sid3)
{
    try {
        std::string url = "https://api.etf2l.org/player/" + sid3 + ".json";
        std::string content = download_url(url);
        json j = json::parse(content);
        std::string alias = j["player"]["name"].get<std::string>();
        return alias;
    } catch (std::exception& e) {
        return std::string(sid3);
    }

}

}
