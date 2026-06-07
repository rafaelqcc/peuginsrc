#include "runtime.hpp"
#include <chrono>
#include <cmath>
#include <sstream>
#include <thread>

CheatRuntime::CheatRuntime(GameSession session) : session_(std::move(session)) {
    auto [w, h] = overlay_.display_size();
    if (w >= 64 && h >= 64) {
        display_w_ = w;
        display_h_ = h;
    }
    overlay_.set_crosshair_size(ch_size_.load());
    overlay_.set_crosshair_thickness(ch_thickness_.load());
    overlay_.set_crosshair_gap(ch_gap_.load());
    overlay_.set_crosshair_color(ch_r_.load(), ch_g_.load(), ch_b_.load());
}

std::string CheatRuntime::status_line() const {
    std::ostringstream oss;
    oss << "ESP: " << (esp_on_.load() ? "ON" : "OFF")
        << "  AIMBOT: "
        << (aimbot_on_.load() ? "ON " + std::to_string(static_cast<int>(aim_speed_.load())) : "OFF")
        << "  WALKSPEED: "
        << (ws_on_.load() ? "ON " + std::to_string(static_cast<int>(ws_speed_.load())) : "OFF")
        << "  JUMP: "
        << (jp_on_.load() ? "ON " + std::to_string(static_cast<int>(jp_power_.load())) : "OFF");
    return oss.str();
}

std::pair<int, int> CheatRuntime::display_size() {
    auto [ow, oh] = overlay_.display_size();
    if (ow >= 64 && oh >= 64) return {ow, oh};
    return {display_w_, display_h_};
}

void CheatRuntime::ensure_pos_offset() {
    {
        std::lock_guard lock(pos_offset_mutex_);
        if (session_.pos_offset) return;
    }
    auto dm = get_datamodel(session_.mem, session_.base);
    auto players = find_players_service(session_.mem, dm);
    if (!players) return;
    auto local = session_.mem.read_u64(*players + offsets::PLAYERS_LOCAL_PLAYER);
    auto character = session_.mem.read_u64(local + offsets::PLAYER_CHARACTER);
    auto off = calibrate_pos_offset(session_.mem, character);
    if (!off) return;
    std::lock_guard lock(pos_offset_mutex_);
    if (!session_.pos_offset) session_.pos_offset = off;
}

void CheatRuntime::apply_walkspeed() {
    auto dm = get_datamodel(session_.mem, session_.base);
    auto humanoid = get_local_humanoid(session_.mem, dm);
    ::set_walkspeed(session_.mem, humanoid, ws_speed_.load());
}

void CheatRuntime::esp_update_loop() {
    while (!esp_thread_stop_) {
        try {
            ensure_pos_offset();
            std::optional<std::uint64_t> pos_off;
            {
                std::lock_guard lock(pos_offset_mutex_);
                pos_off = session_.pos_offset;
            }

            auto snap = read_game_snapshot(session_.mem, session_.base);

            if (++world_refresh_counter_ >= kWorldRefreshInterval) {
                world_refresh_counter_ = 0;
                refresh_esp_world_cache(session_.mem, snap.datamodel, snap.local_player,
                                        pos_off, world_cache_);
            }

            auto [sw, sh] = display_size();
            auto boxes = project_esp_world_cache(session_.mem, snap, world_cache_,
                                                  session_.pid, sw, sh,
                                                  esp_max_dist_.load(std::memory_order_relaxed));
            {
                std::lock_guard lock(esp_cache_lock_);
                esp_cache_ = std::move(boxes);
            }
        } catch (...) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

std::vector<EspBox> CheatRuntime::esp_boxes_for_draw() {
    if (!esp_on_.load() || stop_.load()) return {};
    std::lock_guard lock(esp_cache_lock_);
    auto out = esp_cache_;
    float r = esp_r_.load(std::memory_order_relaxed);
    float g = esp_g_.load(std::memory_order_relaxed);
    float b = esp_b_.load(std::memory_order_relaxed);
    bool fill = esp_fill_on_.load(std::memory_order_relaxed);
    float a = esp_a_.load(std::memory_order_relaxed);
    for (auto& box : out) {
        box.r = r; box.g = g; box.b = b;
        box.fill = fill; box.a = a;
    }
    return out;
}

int CheatRuntime::overlay_fps() const {
    return overlay_.draw_interval_ms() > 0 ? 1000 / overlay_.draw_interval_ms() : 240;
}

void CheatRuntime::set_overlay_fps(int fps) {
    int ms = std::clamp(1000 / std::max(fps, 1), 1, 100);
    overlay_.set_draw_interval(ms);
}

void CheatRuntime::set_esp_color(float r, float g, float b) {
    esp_r_.store(r);
    esp_g_.store(g);
    esp_b_.store(b);
}

void CheatRuntime::set_crosshair_on(bool v) {
    ch_on_.store(v);
    if (v) {
        if (!overlay_.is_running()) {
            overlay_.set_box_factory([] { return std::vector<EspBox>(); });
            overlay_.show(false, 0);
            overlay_.clear_box_factory();
        } else {
            overlay_.show();
        }
        overlay_.set_crosshair_enabled(true);
    } else {
        overlay_.set_crosshair_enabled(false);
        if (!esp_on_.load()) overlay_.hide();
    }
}

void CheatRuntime::set_crosshair_size(float v) {
    ch_size_.store(v);
    overlay_.set_crosshair_size(v);
}

void CheatRuntime::set_crosshair_thickness(float v) {
    ch_thickness_.store(v);
    overlay_.set_crosshair_thickness(v);
}

void CheatRuntime::set_crosshair_gap(float v) {
    ch_gap_.store(v);
    overlay_.set_crosshair_gap(v);
}

void CheatRuntime::set_crosshair_color(float r, float g, float b) {
    ch_r_.store(r);
    ch_g_.store(g);
    ch_b_.store(b);
    overlay_.set_crosshair_color(r, g, b);
}

void CheatRuntime::apply_jumppower() {
    auto dm = get_datamodel(session_.mem, session_.base);
    auto humanoid = get_local_humanoid(session_.mem, dm);
    ::set_jumppower(session_.mem, humanoid, jp_power_.load());
}

void CheatRuntime::jumppower_loop() {
    while (!stop_) {
        if (jp_on_.load()) {
            try {
                apply_jumppower();
            } catch (...) {
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
    }
}

void CheatRuntime::walkspeed_loop() {
    while (!stop_) {
        if (ws_on_.load()) {
            try {
                apply_walkspeed();
            } catch (...) {
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
    }
}

void CheatRuntime::aim_loop() {
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    constexpr auto frame = std::chrono::microseconds(4000);

    while (!stop_) {
        if (aim_fov_on_.load(std::memory_order_relaxed)) {
            auto [sw, sh] = display_size();
            float fov = aim_fov_.load(std::memory_order_relaxed);
            float fr = aim_fov_r_.load(std::memory_order_relaxed);
            float fg = aim_fov_g_.load(std::memory_order_relaxed);
            float fb = aim_fov_b_.load(std::memory_order_relaxed);
            float fa = aim_fov_a_.load(std::memory_order_relaxed);
            overlay_.set_fov_circle(sw * 0.5f, sh * 0.5f, fov, fr, fg, fb, fa);
        } else {
            overlay_.clear_fov_circle();
        }

        if (!aimbot_on_.load() || !mouse_.mouse5_held()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        try {
            ensure_pos_offset();
            std::optional<std::uint64_t> pos_off;
            {
                std::lock_guard lock(pos_offset_mutex_);
                pos_off = session_.pos_offset;
            }
            auto snap = read_game_snapshot(session_.mem, session_.base);
            auto [sw, sh] = display_size();
            auto targets = collect_aim_targets_snapshot(session_.mem, snap, pos_off);
            auto mapper = ViewportMapper::for_session(session_.pid, snap.game_w, snap.game_h, sw, sh);
            std::vector<std::pair<float, float>> mapped;
            mapped.reserve(targets.size());
            for (const auto& [tx, ty] : targets)
                mapped.push_back(mapper.game_to_screen(tx, ty));
            auto [cx, cy] = mapper.crosshair();
            const float speed = aim_speed_.load();
            float fov = aim_fov_.load(std::memory_order_relaxed);
            if (auto delta = pick_nearest_target(mapped, static_cast<float>(cx),
                                                 static_cast<float>(cy), fov)) {
                float dx = delta->first, dy = delta->second;
                if (std::hypot(dx, dy) >= aim_deadzone_) {
                    float factor = std::clamp(speed / 100.f, 0.01f, 1.f);
                    float step = std::max(2.f, speed * 0.4f);
                    float mx = std::clamp(dx * factor, -step, step);
                    float my = std::clamp(dy * factor, -step, step);
                    mouse_.move_relative(mx, my);
                }
            }
        } catch (...) {
        }
        next += frame;
        std::this_thread::sleep_until(next);
    }
}

void CheatRuntime::start_workers() {
    ws_thread_ = std::thread(&CheatRuntime::walkspeed_loop, this);
    jp_thread_ = std::thread(&CheatRuntime::jumppower_loop, this);
    aim_thread_ = std::thread(&CheatRuntime::aim_loop, this);
}

void CheatRuntime::shutdown() {
    stop_ = true;
    esp_on_ = false;
    esp_thread_stop_ = true;
    aimbot_on_ = false;
    ws_on_ = false;
    jp_on_ = false;
    if (esp_thread_.joinable()) esp_thread_.join();
    if (aim_thread_.joinable()) aim_thread_.join();
    if (ws_thread_.joinable()) ws_thread_.join();
    if (jp_thread_.joinable()) jp_thread_.join();
    overlay_.clear_box_factory();
    overlay_.stop();
    mouse_.stop();
}

std::string CheatRuntime::set_esp(bool on) {
    esp_on_.store(on);
    if (on) {
        overlay_.set_box_factory([this] { return esp_boxes_for_draw(); });
        auto note = overlay_.show(true, 8.0);
        if (note.empty()) return "ESP failed - overlay not available (Wayland+layer-shell required)";
        esp_thread_stop_ = false;
        if (esp_thread_.joinable()) esp_thread_.join();
        esp_thread_ = std::thread(&CheatRuntime::esp_update_loop, this);
        return "ESP is now on\n" + note;
    }
    esp_thread_stop_ = true;
    if (esp_thread_.joinable()) esp_thread_.join();
    if (!ch_on_.load()) overlay_.hide();
    overlay_.clear_box_factory();
    {
        std::lock_guard lock(esp_cache_lock_);
        esp_cache_.clear();
    }
    return "ESP is now off";
}

std::string CheatRuntime::set_jumppower(bool on, std::optional<float> power) {
    if (power) {
        if (*power <= 0) return "Jump power must be > 0";
        jp_power_.store(*power);
    }
    jp_on_.store(on);
    if (on) {
        try {
            apply_jumppower();
            return "Jump power on at " + std::to_string(static_cast<int>(jp_power_.load()));
        } catch (const std::exception& e) {
            return std::string("Jump power failed: ") + e.what();
        }
    }
    return "Jump power off";
}

std::string CheatRuntime::set_walkspeed(bool on, std::optional<float> speed) {
    if (speed) {
        if (*speed <= 0) return "Speed must be > 0";
        ws_speed_.store(*speed);
    }
    ws_on_.store(on);
    if (on) {
        try {
            apply_walkspeed();
            return "Walkspeed on at " + std::to_string(static_cast<int>(ws_speed_.load()));
        } catch (const std::exception& e) {
            return std::string("Walkspeed failed: ") + e.what();
        }
    }
    return "Walkspeed off";
}

std::string CheatRuntime::set_aimbot(bool on) {
    if (on) {
        if (!mouse_.start()) return "Aimbot failed: " + mouse_.last_error();
        aimbot_on_.store(true);
        std::string msg = "Aimbot on — hold mouse5/side (" + mouse_.backend() + ")";
        if (!mouse_.listener_ok())
            msg += "\n(mouse listener failed: " + mouse_.last_error() + ")";
        else if (!mouse_.has_side_buttons())
            msg += "\n(side buttons not detected — hold RIGHT mouse to aim)";
        return msg;
    }
    aimbot_on_.store(false);
    mouse_.stop();
    return "Aimbot off";
}

std::string CheatRuntime::set_aimbot_speed(float speed) {
    if (speed <= 0 || speed > 100) return "Aimbot speed must be 1-100";
    aim_speed_.store(speed);
    return "Aimbot speed " + std::to_string(static_cast<int>(speed));
}

void CheatRuntime::set_aim_fov_color(float r, float g, float b) {
    aim_fov_r_.store(r, std::memory_order_relaxed);
    aim_fov_g_.store(g, std::memory_order_relaxed);
    aim_fov_b_.store(b, std::memory_order_relaxed);
}

void CheatRuntime::set_aim_fov_alpha(float a) {
    aim_fov_a_.store(a, std::memory_order_relaxed);
}



