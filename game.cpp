#include "game.hpp"
#include "offsets.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

std::optional<std::uint64_t> try_datamodel_visual(Memory& mem, std::uint64_t base) {
    try {
        auto visual = mem.read_u64(base + offsets::VISUAL_ENGINE_POINTER);
        if (!looks_like_ptr(visual)) return std::nullopt;
        auto fake = mem.read_u64(visual + offsets::VISUAL_ENGINE_FAKE_DATAMODEL);
        if (!looks_like_ptr(fake)) return std::nullopt;
        auto dm = mem.read_u64(fake + offsets::FAKE_DATAMODEL_REAL_DATAMODEL);
        if (!looks_like_ptr(dm)) return std::nullopt;
        return dm;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::uint64_t> try_datamodel_fake(Memory& mem, std::uint64_t base) {
    try {
        auto fake = mem.read_u64(base + offsets::FAKE_DATAMODEL_POINTER);
        if (!looks_like_ptr(fake)) return std::nullopt;
        auto dm = mem.read_u64(fake + offsets::FAKE_DATAMODEL_REAL_DATAMODEL);
        if (!looks_like_ptr(dm)) return std::nullopt;
        return dm;
    } catch (...) {
        return std::nullopt;
    }
}

std::uint64_t get_datamodel(Memory& mem, std::uint64_t base) {
    if (auto dm = try_datamodel_visual(mem, base)) return *dm;
    if (auto dm = try_datamodel_fake(mem, base)) return *dm;
    throw MemError("DataModel chain broke");
}

std::vector<std::uint64_t> get_children(Memory& mem, std::uint64_t instance) {
    std::vector<std::uint64_t> out;
    auto container = mem.read_u64(instance + offsets::INSTANCE_CHILDREN_START);
    if (!container) return out;
    auto start = mem.read_u64(container);
    auto end = mem.read_u64(container + offsets::INSTANCE_CHILDREN_END);
    std::uint64_t ptr = start;
    int guard = 0;
    while (ptr < end && guard < 4096) {
        auto child = mem.read_u64(ptr);
        if (child) out.push_back(child);
        ptr += offsets::CHILD_ENTRY_SIZE;
        ++guard;
    }
    return out;
}

std::optional<std::uint64_t> find_players_service(Memory& mem, std::uint64_t datamodel) {
    for (auto child : get_children(mem, datamodel)) {
        try {
            auto local = mem.read_u64(child + offsets::PLAYERS_LOCAL_PLAYER);
            if (!looks_like_heap_ptr(local)) continue;
            auto character = mem.read_u64(local + offsets::PLAYER_CHARACTER);
            if (!looks_like_heap_ptr(character)) continue;
            if (find_humanoid_on_character(mem, character)) return child;
        } catch (...) {
        }
    }
    return std::nullopt;
}

std::optional<std::uint64_t> find_humanoid_on_character(Memory& mem, std::uint64_t character) {
    for (auto child : get_children(mem, character)) {
        try {
            float ws = mem.read_f32(child + offsets::HUMANOID_WALK_SPEED);
            if (ws >= 4.f && ws <= 500.f) return child;
        } catch (...) {
        }
    }
    return std::nullopt;
}

std::uint64_t get_local_humanoid(Memory& mem, std::uint64_t datamodel) {
    auto players = find_players_service(mem, datamodel);
    if (!players) throw MemError("Players service not found");
    auto local = mem.read_u64(*players + offsets::PLAYERS_LOCAL_PLAYER);
    if (!local) throw MemError("LocalPlayer is null");
    auto character = mem.read_u64(local + offsets::PLAYER_CHARACTER);
    if (!character) throw MemError("Character is null");
    auto humanoid = find_humanoid_on_character(mem, character);
    if (!humanoid) throw MemError("Humanoid not found");
    return *humanoid;
}

void set_walkspeed(Memory& mem, std::uint64_t humanoid, float speed) {
    mem.write_f32(humanoid + offsets::HUMANOID_WALK_SPEED, speed);
    mem.write_f32(humanoid + offsets::HUMANOID_WALK_SPEED_CHECK, speed);
}

void set_jumppower(Memory& mem, std::uint64_t humanoid, float power) {
    mem.write_f32(humanoid + offsets::HUMANOID_JUMP_POWER, power);
    uint8_t one = 1;
    mem.write_bytes(humanoid + offsets::HUMANOID_USE_JUMP_POWER, &one, 1);
}

void set_target_point(Memory& mem, std::uint64_t humanoid, const Vec3& pos) {
    mem.write_f32(humanoid + offsets::Humanoid::TargetPoint, pos.x);
    mem.write_f32(humanoid + offsets::Humanoid::TargetPoint + 4, pos.y);
    mem.write_f32(humanoid + offsets::Humanoid::TargetPoint + 8, pos.z);
}

std::uint64_t get_visual_engine(Memory& mem, std::uint64_t base) {
    auto ve = mem.read_u64(base + offsets::VISUAL_ENGINE_POINTER);
    if (!looks_like_heap_ptr(ve)) throw MemError("VisualEngine invalid");
    return ve;
}

std::array<float, 16> get_view_matrix(Memory& mem, std::uint64_t visual_engine) {
    auto b = mem.read_bytes(visual_engine + offsets::VISUAL_ENGINE_VIEW_MATRIX, 64);
    std::array<float, 16> m{};
    std::memcpy(m.data(), b.data(), 64);
    return m;
}

std::pair<float, float> get_screen_dims(Memory& mem, std::uint64_t visual_engine) {
    float w = mem.read_f32(visual_engine + offsets::VISUAL_ENGINE_DIMENSIONS);
    float h = mem.read_f32(visual_engine + offsets::VISUAL_ENGINE_DIMENSIONS + 4);
    if (w < 64 || h < 64) return {1920.f, 1080.f};
    return {w, h};
}

namespace {

struct Rgb8 {
    std::uint8_t r{}, g{}, b{};
};

Rgb8 brick_color_to_rgb(std::uint32_t brick) {
    switch (brick) {
        case 1:
            return {242, 243, 243};
        case 21:
            return {196, 40, 28};
        case 23:
            return {13, 105, 172};
        case 24:
            return {18, 128, 71};
        case 26:
            return {245, 205, 48};
        case 29:
            return {98, 37, 209};
        case 37:
            return {232, 186, 200};
        case 101:
            return {0, 32, 96};
        case 119:
            return {170, 0, 0};
        case 125:
            return {170, 85, 0};
        case 192:
            return {248, 248, 248};
        case 194:
            return {163, 162, 165};
        default:
            break;
    }
    const std::uint32_t h = brick * 2654435761u;
    return {static_cast<std::uint8_t>(80 + (h & 0x7F)),
            static_cast<std::uint8_t>(80 + ((h >> 8) & 0x7F)),
            static_cast<std::uint8_t>(80 + ((h >> 16) & 0x7F))};
}

std::optional<std::string> read_roblox_string_field(Memory& mem, std::uint64_t field_addr) {
    try {
        const auto ptr = mem.read_u64(field_addr);
        if (!looks_like_heap_ptr(ptr)) return std::nullopt;
        std::string out;
        out.reserve(32);
        for (int i = 0; i < 64; ++i) {
            const auto ch = static_cast<char>(mem.read_bytes(ptr + static_cast<std::uint64_t>(i), 1)[0]);
            if (ch == '\0') break;
            if (ch < 0x20 || ch > 0x7e) return std::nullopt;
            out.push_back(ch);
        }
        return out.empty() ? std::nullopt : std::optional<std::string>(out);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> read_instance_class_name(Memory& mem, std::uint64_t instance) {
    try {
        const auto desc = mem.read_u64(instance + offsets::INSTANCE_CLASS_DESCRIPTOR);
        if (!looks_like_heap_ptr(desc)) return std::nullopt;
        return read_roblox_string_field(mem, desc + offsets::INSTANCE_CLASS_NAME);
    } catch (...) {
        return std::nullopt;
    }
}

bool is_team_instance(Memory& mem, std::uint64_t instance) {
    const auto cls = read_instance_class_name(mem, instance);
    return cls && *cls == "Team";
}

std::optional<std::uint32_t> read_team_brick_color(Memory& mem, std::uint64_t team_inst) {
    if (!is_team_instance(mem, team_inst)) return std::nullopt;
    try {
        const auto color = mem.read_u32(team_inst + offsets::TEAM_BRICK_COLOR);
        if (color < 2000) return color;
    } catch (...) {
    }
    return std::nullopt;
}

// Every offset constant from alltheoffsets.h (used to locate Player.Team without adding it).
constexpr std::uint64_t kDumpInstancePtrOffsets[] = {
    offsets::Instance::AttributeContainer, offsets::Instance::AttributeList,
    offsets::Instance::AttributeToNext,    offsets::Instance::AttributeToValue,
    offsets::Instance::ChildrenEnd,        offsets::Instance::ChildrenStart,
    offsets::Instance::ClassDescriptor,      offsets::Instance::ClassName,
    offsets::Instance::Name,               offsets::Instance::Parent,
    offsets::Player::Character,            offsets::Players::LocalPlayer,
    offsets::DataModel::Workspace,         offsets::Humanoid::SeatPart,
};

std::vector<std::pair<std::uint64_t, std::uint32_t>> collect_teams(Memory& mem,
                                                                     std::uint64_t datamodel) {
    std::vector<std::pair<std::uint64_t, std::uint32_t>> teams;
    for (auto child : get_children(mem, datamodel)) {
        if (!looks_like_heap_ptr(child)) continue;
        if (is_team_instance(mem, child)) {
            if (auto c = read_team_brick_color(mem, child)) teams.emplace_back(child, *c);
            continue;
        }
        const auto cls = read_instance_class_name(mem, child);
        if (!cls || *cls != "Teams") continue;
        for (auto team_inst : get_children(mem, child)) {
            if (auto c = read_team_brick_color(mem, team_inst)) teams.emplace_back(team_inst, *c);
        }
    }
    return teams;
}

std::optional<std::uint32_t> team_color_for_player(
    Memory& mem, std::uint64_t player_inst,
    const std::vector<std::pair<std::uint64_t, std::uint32_t>>& teams) {
    if (teams.empty()) return std::nullopt;
    for (const auto off : kDumpInstancePtrOffsets) {
        try {
            const auto ptr = mem.read_u64(player_inst + off);
            for (const auto& [team_ptr, color] : teams) {
                if (ptr == team_ptr) return color;
            }
        } catch (...) {
        }
    }
    return std::nullopt;
}

constexpr float kBodyXZRadius = 3.0f;
constexpr float kBodyYRadius = 5.0f;
constexpr float kMaxBodyXZSpread = 3.5f;
constexpr float kMaxBodyYSpread = 4.5f;

Vec3 centroid(const std::vector<Vec3>& pts) {
    Vec3 c{};
    for (const auto& p : pts) {
        c.x += p.x;
        c.y += p.y;
        c.z += p.z;
    }
    const float inv = 1.f / static_cast<float>(pts.size());
    c.x *= inv;
    c.y *= inv;
    c.z *= inv;
    return c;
}

std::vector<Vec3> filter_positions_near_centroid(std::vector<Vec3> positions) {
    if (positions.size() < 2) return positions;
    const Vec3 c = centroid(positions);
    std::vector<Vec3> kept;
    kept.reserve(positions.size());
    for (const auto& p : positions) {
        if (std::hypot(p.x - c.x, p.z - c.z) <= kBodyXZRadius &&
            std::abs(p.y - c.y) <= kBodyYRadius)
            kept.push_back(p);
    }
    return kept.size() >= 2 ? kept : positions;
}

void peel_world_outliers(std::vector<Vec3>& positions) {
    while (positions.size() > 3) {
        const Vec3 c = centroid(positions);
        float max_xz = 0.f, max_y = 0.f;
        std::size_t worst = 0;
        float worst_score = 0.f;
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const float xz = std::hypot(positions[i].x - c.x, positions[i].z - c.z);
            const float yd = std::abs(positions[i].y - c.y);
            max_xz = std::max(max_xz, xz);
            max_y = std::max(max_y, yd);
            const float score = xz + yd * 0.35f;
            if (score > worst_score) {
                worst_score = score;
                worst = i;
            }
        }
        if (max_xz <= kMaxBodyXZSpread && max_y <= kMaxBodyYSpread) break;
        positions.erase(positions.begin() + static_cast<std::ptrdiff_t>(worst));
    }
}

std::optional<std::pair<float, float>> world_to_screen_for_box(
    const Vec3& pos, const std::array<float, 16>& m, float width, float height) {
    float qx = pos.x * m[0] + pos.y * m[1] + pos.z * m[2] + m[3];
    float qy = pos.x * m[4] + pos.y * m[5] + pos.z * m[6] + m[7];
    float qw = pos.x * m[12] + pos.y * m[13] + pos.z * m[14] + m[15];
    if (qw < 0.25f) return std::nullopt;
    float ndc_x = qx / qw;
    float ndc_y = qy / qw;
    if (std::abs(ndc_x) > 1.35f || std::abs(ndc_y) > 1.35f) return std::nullopt;
    float sx = (width / 2.f) * ndc_x + (width / 2.f);
    float sy = -(height / 2.f) * ndc_y + (height / 2.f);
    if (!std::isfinite(sx) || !std::isfinite(sy)) return std::nullopt;
    if (sx < -200 || sy < -200 || sx > width + 200 || sy > height + 200) return std::nullopt;
    return std::make_pair(sx, sy);
}

void peel_screen_outliers(std::vector<std::pair<float, float>>& pts) {
    while (pts.size() > 2) {
        float cx = 0, cy = 0;
        for (const auto& [x, y] : pts) {
            cx += x;
            cy += y;
        }
        cx /= static_cast<float>(pts.size());
        cy /= static_cast<float>(pts.size());

        std::vector<float> dists;
        dists.reserve(pts.size());
        for (const auto& [x, y] : pts) dists.push_back(std::hypot(x - cx, y - cy));

        std::vector<float> sorted = dists;
        std::sort(sorted.begin(), sorted.end());
        const float median = sorted[sorted.size() / 2];
        const float limit = std::max(90.f, median * 2.2f);

        std::size_t worst = 0;
        float worst_d = 0.f;
        bool any = false;
        for (std::size_t i = 0; i < pts.size(); ++i) {
            if (dists[i] <= limit) continue;
            any = true;
            if (dists[i] > worst_d) {
                worst_d = dists[i];
                worst = i;
            }
        }
        if (!any) break;
        pts.erase(pts.begin() + static_cast<std::ptrdiff_t>(worst));
    }
}

}  // namespace

void esp_box_rgb_from_team_color(std::uint32_t brick_color, float& r, float& g, float& b) {
    const Rgb8 rgb = brick_color_to_rgb(brick_color);
    r = rgb.r / 255.f;
    g = rgb.g / 255.f;
    b = rgb.b / 255.f;
}

std::optional<std::pair<float, float>> world_to_screen(
    const Vec3& pos, const std::array<float, 16>& m, float width, float height) {
    float qx = pos.x * m[0] + pos.y * m[1] + pos.z * m[2] + m[3];
    float qy = pos.x * m[4] + pos.y * m[5] + pos.z * m[6] + m[7];
    float qw = pos.x * m[12] + pos.y * m[13] + pos.z * m[14] + m[15];
    if (qw < 0.1f) return std::nullopt;
    float ndc_x = qx / qw;
    float ndc_y = qy / qw;
    float sx = (width / 2.f) * ndc_x + (width / 2.f);
    float sy = -(height / 2.f) * ndc_y + (height / 2.f);
    if (!std::isfinite(sx) || !std::isfinite(sy)) return std::nullopt;
    if (sx < -200 || sy < -200 || sx > width + 200 || sy > height + 200) return std::nullopt;
    return std::make_pair(sx, sy);
}

std::optional<Vec3> read_part_position(Memory& mem, std::uint64_t part,
                                       std::optional<std::uint64_t> pos_offset) {
    try {
        auto prim = mem.read_u64(part + offsets::BASEPART_PRIMITIVE);
        if (!looks_like_heap_ptr(prim)) return std::nullopt;
        if (pos_offset) {
            auto v = mem.read_vec3(prim + *pos_offset);
            return vec3_sane(v) ? std::optional<Vec3>(v) : std::nullopt;
        }
        for (auto off : offsets::POS_OFFSET_CANDIDATES) {
            auto v = mem.read_vec3(prim + off);
            if (vec3_sane(v)) return v;
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::vector<Vec3> collect_character_part_positions(Memory& mem, std::uint64_t character,
                                                   std::optional<std::uint64_t> pos_offset) {
    std::vector<Vec3> positions;
    for (auto child : get_children(mem, character)) {
        if (auto pos = read_part_position(mem, child, pos_offset)) positions.push_back(*pos);
    }
    if (positions.size() < 2) return positions;
    positions = filter_positions_near_centroid(std::move(positions));
    peel_world_outliers(positions);
    return positions;
}

std::optional<std::tuple<float, float, float, float>> character_screen_box(
    const std::vector<Vec3>& positions, const std::array<float, 16>& m, float w, float h) {
    if (positions.size() < 2 || w < 1.f || h < 1.f) return std::nullopt;

    std::vector<std::pair<float, float>> screen_pts;
    screen_pts.reserve(positions.size());
    for (const auto& p : positions) {
        if (auto pt = world_to_screen_for_box(p, m, w, h)) screen_pts.push_back(*pt);
    }
    if (screen_pts.size() < 2) return std::nullopt;

    peel_screen_outliers(screen_pts);
    if (screen_pts.size() < 2) return std::nullopt;

    float left = screen_pts[0].first, right = left, top = screen_pts[0].second, bottom = top;
    for (const auto& [sx, sy] : screen_pts) {
        left = std::min(left, sx);
        right = std::max(right, sx);
        top = std::min(top, sy);
        bottom = std::max(bottom, sy);
    }

    float box_w = right - left;
    float box_h = bottom - top;
    if (box_w < 4.f || box_h < 4.f) return std::nullopt;

    const float pad_x = std::clamp(box_w * 0.06f, 2.f, 12.f);
    const float pad_y = std::clamp(box_h * 0.05f, 2.f, 14.f);
    left -= pad_x;
    right += pad_x;
    top -= pad_y;
    bottom += pad_y;
    box_w = right - left;
    box_h = bottom - top;

    // Reject implausible sizes (bad reads / perspective blow-up).
    if (box_w > w * 0.42f || box_h > h * 0.65f) return std::nullopt;

    // Typical standing character is taller than wide.
    constexpr float kMaxAspect = 1.15f;
    if (box_w > box_h * kMaxAspect) {
        const float target_w = box_h * 0.72f;
        const float cx = (left + right) * 0.5f;
        left = cx - target_w * 0.5f;
        right = cx + target_w * 0.5f;
    }

    if (right - left < 4.f || bottom - top < 4.f) return std::nullopt;
    return std::make_tuple(left, top, right, bottom);
}

std::optional<std::uint64_t> calibrate_pos_offset(Memory& mem, std::uint64_t character) {
    for (auto off : offsets::POS_OFFSET_CANDIDATES) {
        for (auto child : get_children(mem, character)) {
            try {
                auto prim = mem.read_u64(child + offsets::BASEPART_PRIMITIVE);
                if (!looks_like_heap_ptr(prim)) continue;
                auto v = mem.read_vec3(prim + off);
                if (vec3_sane(v)) return off;
            } catch (...) {
            }
        }
    }
    return std::nullopt;
}

GameSnapshot read_game_snapshot(Memory& mem, std::uint64_t base) {
    GameSnapshot s;
    s.datamodel = get_datamodel(mem, base);
    s.visual_engine = get_visual_engine(mem, base);
    s.matrix = get_view_matrix(mem, s.visual_engine);
    std::tie(s.game_w, s.game_h) = get_screen_dims(mem, s.visual_engine);
    if (auto players_svc = find_players_service(mem, s.datamodel)) {
        try {
            s.local_player = mem.read_u64(*players_svc + offsets::PLAYERS_LOCAL_PLAYER);
        } catch (...) {
        }
    }
    return s;
}

static std::vector<std::pair<std::uint64_t, std::uint64_t>> iter_other_players(
    Memory& mem, std::uint64_t datamodel) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> out;
    auto players_svc = find_players_service(mem, datamodel);
    if (!players_svc) return out;
    for (auto child : get_children(mem, *players_svc)) {
        if (!looks_like_heap_ptr(child)) continue;
        try {
            auto character = mem.read_u64(child + offsets::PLAYER_CHARACTER);
            if (!looks_like_heap_ptr(character)) continue;
            if (!find_humanoid_on_character(mem, character)) continue;
            out.emplace_back(child, character);
        } catch (...) {
        }
    }
    return out;
}

static std::string read_player_name(Memory& mem, std::uint64_t player_inst) {
    try {
        // Read a pointer to null-terminated string data at Instance::Name.
        // Works for both heap-allocated and SSO inline names.
        const auto ptr = mem.read_u64(player_inst + offsets::Instance::Name);
        if (ptr == 0) return "";
        std::string out;
        for (int i = 0; i < 32; ++i) {
            const auto ch = static_cast<char>(mem.read_bytes(ptr + static_cast<std::uint64_t>(i), 1)[0]);
            if (ch == '\0') break;
            out.push_back(ch);
        }
        return out;
    } catch (...) {
        return "";
    }
}

void refresh_esp_world_cache(Memory& mem, std::uint64_t datamodel, std::uint64_t local_player,
                             std::optional<std::uint64_t> pos_offset, EspWorldCache& out) {
    auto saved = std::move(out.players);
    out.players.clear();
    try {
        const auto teams = collect_teams(mem, datamodel);
        for (const auto& [player_inst, character] : iter_other_players(mem, datamodel)) {
            if (player_inst == local_player) continue;
            auto parts = collect_character_part_positions(mem, character, pos_offset);
            if (parts.size() < 2) continue;
            EspCachedPlayer entry;
            entry.parts = std::move(parts);
            if (auto c = team_color_for_player(mem, player_inst, teams)) entry.team_color = *c;
            entry.name = read_player_name(mem, player_inst);
            out.players.push_back(std::move(entry));
        }
    } catch (...) {
        out.players = std::move(saved);
        throw;
    }
}

std::vector<EspBox> project_esp_world_cache(Memory& mem, const GameSnapshot& snap,
                                            const EspWorldCache& world, int pid, int screen_w,
                                            int screen_h, float max_qw) {
    if (world.players.empty()) return {};
    auto mapper = ViewportMapper::for_session(pid, snap.game_w, snap.game_h, screen_w, screen_h);
    const auto& m = snap.matrix;
    std::vector<EspBox> boxes;
    boxes.reserve(world.players.size());
    for (const auto& entry : world.players) {
        if (entry.parts.empty()) continue;
        Vec3 centroid{};
        for (const auto& p : entry.parts) {
            centroid.x += p.x; centroid.y += p.y; centroid.z += p.z;
        }
        float inv = 1.f / static_cast<float>(entry.parts.size());
        centroid.x *= inv; centroid.y *= inv; centroid.z *= inv;
        float qw = centroid.x * m[12] + centroid.y * m[13] + centroid.z * m[14] + m[15];
        if (qw > max_qw) continue;
        auto rect = character_screen_box(entry.parts, snap.matrix, snap.game_w, snap.game_h);
        if (!rect) continue;
        auto [left, top, right, bottom] = *rect;
        auto [x1, y1] = mapper.game_to_screen(left, top);
        auto [x2, y2] = mapper.game_to_screen(right, bottom);
        if (x1 > x2) std::swap(x1, x2);
        if (y1 > y2) std::swap(y1, y2);
        const float bw = x2 - x1, bh = y2 - y1;
        if (bw > static_cast<float>(screen_w) * 0.45f ||
            bh > static_cast<float>(screen_h) * 0.7f)
            continue;
        EspBox box{x1, y1, x2, y2, {}, {}, {}, {}, {}, {}};
        box.name = entry.name;
        boxes.push_back(box);
    }
    return boxes;
}

std::vector<EspBox> collect_esp_boxes_snapshot(
    Memory& mem, const GameSnapshot& snap, std::optional<std::uint64_t> pos_offset) {
    std::vector<EspBox> boxes;
    for (const auto& [player_inst, character] : iter_other_players(mem, snap.datamodel)) {
        if (player_inst == snap.local_player) continue;
        auto parts = collect_character_part_positions(mem, character, pos_offset);
        if (parts.size() < 2) continue;
        auto rect = character_screen_box(parts, snap.matrix, snap.game_w, snap.game_h);
        if (!rect) continue;
        auto [left, top, right, bottom] = *rect;
        boxes.push_back({left, top, right, bottom, {}, {}, {}, {}, {}, {}});
    }
    return boxes;
}

std::vector<std::pair<float, float>> collect_aim_targets_snapshot(
    Memory& mem, const GameSnapshot& snap, std::optional<std::uint64_t> pos_offset) {
    std::vector<std::pair<float, float>> targets;
    for (const auto& [player_inst, character] : iter_other_players(mem, snap.datamodel)) {
        if (player_inst == snap.local_player) continue;
        auto parts = collect_character_part_positions(mem, character, pos_offset);
        if (parts.size() < 2) continue;
        auto rect = character_screen_box(parts, snap.matrix, snap.game_w, snap.game_h);
        if (!rect) continue;
        auto [left, top, right, bottom] = *rect;
        float cx = (left + right) * 0.5f;
        float cy = top + (bottom - top) * 0.12f;
        targets.emplace_back(cx, cy);
    }
    return targets;
}

std::vector<Vec3> collect_world_targets_snapshot(
    Memory& mem, const GameSnapshot& snap, std::optional<std::uint64_t> pos_offset) {
    std::vector<Vec3> targets;
    for (const auto& [player_inst, character] : iter_other_players(mem, snap.datamodel)) {
        if (player_inst == snap.local_player) continue;
        auto parts = collect_character_part_positions(mem, character, pos_offset);
        if (parts.size() < 2) continue;
        Vec3 c{};
        for (const auto& p : parts) { c.x += p.x; c.y += p.y; c.z += p.z; }
        float inv = 1.f / static_cast<float>(parts.size());
        c.x *= inv; c.y *= inv; c.z *= inv;
        targets.push_back(c);
    }
    return targets;
}

std::vector<EspBox> collect_esp_boxes(Memory& mem, std::uint64_t base,
                                      std::optional<std::uint64_t> pos_offset) {
    return collect_esp_boxes_snapshot(mem, read_game_snapshot(mem, base), pos_offset);
}

std::vector<std::pair<float, float>> collect_aim_targets(
    Memory& mem, std::uint64_t base, std::optional<std::uint64_t> pos_offset) {
    return collect_aim_targets_snapshot(mem, read_game_snapshot(mem, base), pos_offset);
}

ViewportMapper ViewportMapper::for_session(int pid, float gw, float gh, int mon_w, int mon_h) {
    if (auto rect = find_game_window_rect(pid)) {
        auto [wx, wy, ww, wh] = *rect;
        return {gw, gh, static_cast<float>(wx), static_cast<float>(wy),
                static_cast<float>(ww), static_cast<float>(wh)};
    }
    return {gw, gh, 0.f, 0.f, static_cast<float>(mon_w), static_cast<float>(mon_h)};
}

std::pair<float, float> ViewportMapper::game_to_screen(float gx, float gy) const {
    if (game_w < 1 || game_h < 1) return {gx, gy};
    return {screen_x + gx * screen_w / game_w, screen_y + gy * screen_h / game_h};
}

std::pair<float, float> ViewportMapper::crosshair() const {
    return {screen_x + screen_w * 0.5f, screen_y + screen_h * 0.5f};
}

std::vector<EspBox> scale_boxes_to_display(const std::vector<EspBox>& boxes, float game_w,
                                           float game_h, int screen_w, int screen_h, int pid) {
    auto mapper = ViewportMapper::for_session(pid, game_w, game_h, screen_w, screen_h);
    std::vector<EspBox> out;
    out.reserve(boxes.size());
    for (const auto& b : boxes) {
        auto [x1, y1] = mapper.game_to_screen(b.x1, b.y1);
        auto [x2, y2] = mapper.game_to_screen(b.x2, b.y2);
        EspBox mapped{x1, y1, x2, y2, b.r, b.g, b.b, false, 1.f, b.name};
        out.push_back(mapped);
    }
    return out;
}

std::optional<std::pair<float, float>> pick_nearest_target(
    const std::vector<std::pair<float, float>>& targets, float cx, float cy, float max_dist) {
    std::optional<std::tuple<float, float, float>> best;
    for (const auto& [tx, ty] : targets) {
        float dx = tx - cx, dy = ty - cy;
        float dist = std::hypot(dx, dy);
        if (dist > max_dist) continue;
        if (!best || dist < std::get<2>(*best)) best = {dx, dy, dist};
    }
    if (!best) return std::nullopt;
    return std::make_pair(std::get<0>(*best), std::get<1>(*best));
}
