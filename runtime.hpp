#pragma once
#include "game.hpp"
#include "mouse.hpp"
#include "overlay.hpp"
#include "process.hpp"
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

class CheatRuntime {
public:
    explicit CheatRuntime(GameSession session);

    std::string status_line() const;
    std::string set_esp(bool on);
    std::string set_walkspeed(bool on, std::optional<float> speed = std::nullopt);
    std::string set_jumppower(bool on, std::optional<float> power = std::nullopt);
    std::string set_aimbot(bool on);
    std::string set_aimbot_speed(float speed);
    void set_aim_fov(float v) { aim_fov_.store(v, std::memory_order_relaxed); }
    void set_aim_fov_on(bool v) { aim_fov_on_.store(v, std::memory_order_relaxed); }
    void set_aim_fov_color(float r, float g, float b);
    void set_aim_fov_alpha(float a);
    void shutdown();
    void start_workers();

    int pid() const { return session_.pid; }
    std::uint64_t base() const { return session_.base; }

    bool esp_on() const { return esp_on_.load(std::memory_order_relaxed); }
    bool ws_on() const { return ws_on_.load(std::memory_order_relaxed); }
    bool jp_on() const { return jp_on_.load(std::memory_order_relaxed); }
    bool aimbot_on() const { return aimbot_on_.load(std::memory_order_relaxed); }
    float aim_speed() const { return aim_speed_.load(std::memory_order_relaxed); }
    float aim_fov() const { return aim_fov_.load(std::memory_order_relaxed); }
    bool aim_fov_on() const { return aim_fov_on_.load(std::memory_order_relaxed); }
    float aim_fov_r() const { return aim_fov_r_.load(std::memory_order_relaxed); }
    float aim_fov_g() const { return aim_fov_g_.load(std::memory_order_relaxed); }
    float aim_fov_b() const { return aim_fov_b_.load(std::memory_order_relaxed); }
    float aim_fov_a() const { return aim_fov_a_.load(std::memory_order_relaxed); }
    float ws_speed() const { return ws_speed_.load(std::memory_order_relaxed); }
    float jp_power() const { return jp_power_.load(std::memory_order_relaxed); }
    void set_ws_speed(float speed) { ws_speed_.store(speed); }
    void set_jp_power(float power) { jp_power_.store(power); }

    float esp_r() const { return esp_r_.load(std::memory_order_relaxed); }
    float esp_g() const { return esp_g_.load(std::memory_order_relaxed); }
    float esp_b() const { return esp_b_.load(std::memory_order_relaxed); }
    void set_esp_color(float r, float g, float b);
    bool esp_fill_on() const { return esp_fill_on_.load(std::memory_order_relaxed); }
    float esp_a() const { return esp_a_.load(std::memory_order_relaxed); }
    void set_esp_fill(bool v) { esp_fill_on_.store(v, std::memory_order_relaxed); }
    void set_esp_fill_alpha(float a) { esp_a_.store(a, std::memory_order_relaxed); }
    float esp_max_dist() const { return esp_max_dist_.load(std::memory_order_relaxed); }
    void set_esp_max_dist(float v) { esp_max_dist_.store(v, std::memory_order_relaxed); }
    int overlay_fps() const;
    void set_overlay_fps(int fps);

    bool crosshair_on() const { return ch_on_.load(std::memory_order_relaxed); }
    float crosshair_size() const { return ch_size_.load(std::memory_order_relaxed); }
    float crosshair_thickness() const { return ch_thickness_.load(std::memory_order_relaxed); }
    float crosshair_gap() const { return ch_gap_.load(std::memory_order_relaxed); }
    float ch_r() const { return ch_r_.load(std::memory_order_relaxed); }
    float ch_g() const { return ch_g_.load(std::memory_order_relaxed); }
    float ch_b() const { return ch_b_.load(std::memory_order_relaxed); }
    void set_crosshair_on(bool v);
    void set_crosshair_size(float v);
    void set_crosshair_thickness(float v);
    void set_crosshair_gap(float v);
    void set_crosshair_color(float r, float g, float b);

private:
    GameSession session_;
    std::atomic<bool> esp_on_{false};
    std::atomic<bool> ws_on_{false};
    std::atomic<bool> jp_on_{false};
    std::atomic<bool> aimbot_on_{false};
    std::atomic<float> aim_speed_{8.f};
    std::atomic<float> aim_fov_{450.f};
    std::atomic<bool> aim_fov_on_{false};
    std::atomic<float> aim_fov_r_{0.816f};
    std::atomic<float> aim_fov_g_{1.f};
    std::atomic<float> aim_fov_b_{1.f};
    std::atomic<float> aim_fov_a_{0.11f};
    float aim_deadzone_{4.f};
    std::atomic<float> ws_speed_{16.f};
    std::atomic<float> jp_power_{50.f};

    std::atomic<float> esp_r_{0.816f};
    std::atomic<float> esp_g_{1.f};
    std::atomic<float> esp_b_{1.f};
    std::atomic<bool> esp_fill_on_{true};
    std::atomic<float> esp_a_{0.11f};
    std::atomic<float> esp_max_dist_{20000.f};

    std::atomic<bool> ch_on_{false};
    std::atomic<float> ch_size_{12.f};
    std::atomic<float> ch_thickness_{2.f};
    std::atomic<float> ch_gap_{4.f};
    std::atomic<float> ch_r_{0.f};
    std::atomic<float> ch_g_{1.f};
    std::atomic<float> ch_b_{0.f};

    std::atomic<bool> stop_{false};
    std::mutex pos_offset_mutex_;
    EspOverlay overlay_;
    MouseInput mouse_;
    int display_w_{1920};
    int display_h_{1080};

    std::thread ws_thread_;
    std::thread jp_thread_;
    std::thread aim_thread_;
    std::thread esp_thread_;

    std::vector<EspBox> esp_cache_;
    std::mutex esp_cache_lock_;
    std::atomic<bool> esp_thread_stop_{false};

    EspWorldCache world_cache_;
    int world_refresh_counter_{0};
    static constexpr int kWorldRefreshInterval = 1;

    void esp_update_loop();

    void ensure_pos_offset();
    void apply_walkspeed();
    void walkspeed_loop();
    void apply_jumppower();
    void jumppower_loop();
    void aim_loop();
    std::pair<int, int> display_size();
    std::vector<EspBox> esp_boxes_for_draw();
};
