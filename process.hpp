#pragma once
#include "memory.hpp"
#include <optional>
#include <string>
#include <tuple>
#include <vector>

struct MapRow {
    std::uint64_t lo{}, hi{}, file_off{};
    std::string perms;
    std::string path;
};

struct GameSession {
    int pid{};
    Memory mem;
    std::uint64_t base{};
    std::string base_tag;
    std::optional<std::uint64_t> pos_offset;

    GameSession(int pid, std::uint64_t base, std::string tag);
    GameSession(GameSession&&) = default;
    GameSession& operator=(GameSession&&) = default;
};

std::vector<MapRow> parse_maps(int pid);
bool has_base_apk(int pid);
std::string proc_comm(int pid);
std::vector<std::tuple<int, std::string, std::string, std::uint64_t>> list_apk_processes();
int find_pid(const std::string& name_hint);
std::vector<std::tuple<std::string, std::uint64_t>> candidate_bases(int pid);
std::pair<std::uint64_t, std::string> resolve_base(Memory& mem);
GameSession connect_session(std::optional<int> pid, const std::string& name_hint);

std::optional<std::tuple<int, int, int, int>> find_game_window_rect(int pid);
