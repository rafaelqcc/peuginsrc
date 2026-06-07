#pragma once
#include "common.hpp"
#include "memory.hpp"
#include "offsets.hpp"
#include "process.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

inline bool looks_like_ptr(std::uint64_t v) { return v >= offsets::MIN_PTR; }
inline bool looks_like_heap_ptr(std::uint64_t v) {
    return v >= offsets::HEAP_PTR_MIN && v <= offsets::HEAP_PTR_MAX;
}

std::optional<std::uint64_t> try_datamodel_visual(Memory& mem, std::uint64_t base);
std::optional<std::uint64_t> try_datamodel_fake(Memory& mem, std::uint64_t base);
std::uint64_t get_datamodel(Memory& mem, std::uint64_t base);
std::vector<std::uint64_t> get_children(Memory& mem, std::uint64_t instance);
std::optional<std::uint64_t> find_players_service(Memory& mem, std::uint64_t datamodel);
std::optional<std::uint64_t> find_humanoid_on_character(Memory& mem, std::uint64_t character);
std::uint64_t get_local_humanoid(Memory& mem, std::uint64_t datamodel);
void set_walkspeed(Memory& mem, std::uint64_t humanoid, float speed);
void set_jumppower(Memory& mem, std::uint64_t humanoid, float power);
void set_target_point(Memory& mem, std::uint64_t humanoid, const Vec3& pos);

std::uint64_t get_visual_engine(Memory& mem, std::uint64_t base);
std::array<float, 16> get_view_matrix(Memory& mem, std::uint64_t visual_engine);
std::pair<float, float> get_screen_dims(Memory& mem, std::uint64_t visual_engine);
std::optional<std::pair<float, float>> world_to_screen(
    const Vec3& pos, const std::array<float, 16>& m, float w, float h);

std::optional<Vec3> read_part_position(Memory& mem, std::uint64_t part,
                                       std::optional<std::uint64_t> pos_offset);
std::vector<Vec3> collect_character_part_positions(Memory& mem, std::uint64_t character,
                                                   std::optional<std::uint64_t> pos_offset);
std::optional<std::tuple<float, float, float, float>> character_screen_box(
    const std::vector<Vec3>& positions, const std::array<float, 16>& m, float w, float h);
std::optional<std::uint64_t> calibrate_pos_offset(Memory& mem, std::uint64_t character);

struct GameSnapshot {
    std::uint64_t datamodel{};
    std::uint64_t visual_engine{};
    std::array<float, 16> matrix{};
    float game_w{1920.f};
    float game_h{1080.f};
    std::uint64_t local_player{};
};

GameSnapshot read_game_snapshot(Memory& mem, std::uint64_t base);

struct EspCachedPlayer {
    std::vector<Vec3> parts;
    std::uint32_t team_color{21};
    std::string name;
};

/** World-space part positions per player (view matrix applied at draw time). */
struct EspWorldCache {
    std::vector<EspCachedPlayer> players;
};

void refresh_esp_world_cache(Memory& mem, std::uint64_t datamodel, std::uint64_t local_player,
                             std::optional<std::uint64_t> pos_offset, EspWorldCache& out);

std::vector<EspBox> project_esp_world_cache(Memory& mem, const GameSnapshot& snap,
                                            const EspWorldCache& world, int pid, int screen_w,
                                            int screen_h, float max_qw = 50000.f);

std::vector<EspBox> collect_esp_boxes_snapshot(
    Memory& mem, const GameSnapshot& snap, std::optional<std::uint64_t> pos_offset);
std::vector<std::pair<float, float>> collect_aim_targets_snapshot(
    Memory& mem, const GameSnapshot& snap, std::optional<std::uint64_t> pos_offset);
std::vector<Vec3> collect_world_targets_snapshot(
    Memory& mem, const GameSnapshot& snap, std::optional<std::uint64_t> pos_offset);

struct ViewportMapper {
    float game_w{}, game_h{};
    float screen_x{}, screen_y{}, screen_w{}, screen_h{};

    static ViewportMapper for_session(int pid, float gw, float gh, int mon_w, int mon_h);
    std::pair<float, float> game_to_screen(float gx, float gy) const;
    std::pair<float, float> crosshair() const;
};

std::vector<EspBox> collect_esp_boxes(Memory& mem, std::uint64_t base,
                                    std::optional<std::uint64_t> pos_offset);
std::vector<std::pair<float, float>> collect_aim_targets(
    Memory& mem, std::uint64_t base, std::optional<std::uint64_t> pos_offset);
std::vector<EspBox> scale_boxes_to_display(const std::vector<EspBox>& boxes, float game_w,
                                           float game_h, int screen_w, int screen_h, int pid);

std::optional<std::pair<float, float>> pick_nearest_target(
    const std::vector<std::pair<float, float>>& targets, float cx, float cy, float max_dist);
