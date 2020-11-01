#pragma once

#include <string>
#include <vector>
#include <map>

#include "include/json.hpp"

namespace autocaptain
{

enum Role
{
    Medic,
    Demoman,
    Pocket,
    PocketScout,
    Roamer,
    FlankScout,
    Offclass,
    None
};

NLOHMANN_JSON_SERIALIZE_ENUM(Role, {
    { Medic, "medic" },
    { Demoman, "demoman" },
    { Pocket, "pocket" },
    { PocketScout, "pocket_scout" },
    { Roamer, "roamer" },
    { FlankScout, "flank_scout" },
    { Offclass, "offclass" },
    { None, "none" }
})

using PlayerID = std::string; // can be their alias or steam ID3

typedef struct
{
    unsigned int games = 0, wins = 0;
    float kpm = 0, apm = 0, dthspm = 0, dpm = 0, dtpm = 0, heals_pc = 0, dpm_pc = 0;
    float hpm = 0, charges = 0, drops = 0;
} PlayerRoleStats;

typedef struct {
    PlayerID alias, sid3;
    std::map<Role, PlayerRoleStats> stats;
} Player;

typedef struct {
    std::uint32_t log_id;
    std::map<Role, PlayerID> blue_team, red_team;
    int blue_rounds, red_rounds;
} Game;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Game, log_id, blue_team, red_team,
                                   blue_rounds, red_rounds)

using nlohmann::json;

class System
{
public:
    System(std::string config_file, bool do_update);
    void AddLog(json log_json, std::uint32_t id);
    void WriteData(std::string data_file, std::string aliases_file);

    float CalculateAbsoluteRating(PlayerRoleStats prs);

    unsigned int GetPlayerGames(PlayerID id, Role r=None);
    float GetPlayerWinrate(PlayerID id, Role r=None);
    PlayerRoleStats GetPlayerStats(PlayerID id, Role r=None);
    std::vector<PlayerID> PlayerLeaderboard(Role r);

    Player* GetPlayer(PlayerID id);

    unsigned int TotalGames();

    std::string name;
private:
    void DownloadLogs(std::string logs_path);
    void FetchAliases(std::string aliases_file);
    // How we get a player's alias. For autocaptain2, this is by retrieving ETF2L alias.
    PlayerID RetrievePlayerAlias(PlayerID sid3);

    std::vector<Player> players;
    std::vector<std::uint32_t> logs_list;
    std::vector<Game> games;
};

}
