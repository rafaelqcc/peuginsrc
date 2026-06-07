#pragma once
#include "common.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <glib.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct _GtkWidget;
struct _GMainContext;
using GtkWidget = struct _GtkWidget;

struct GtkLayerCtx {
    class EspOverlay* self{};
    GtkWidget* win{};
    GtkWidget* draw{};
};

class EspOverlay {
public:
    using BoxFactory = std::function<std::vector<EspBox>()>;

    EspOverlay();
    ~EspOverlay();

    std::pair<int, int> display_size() const;
    void set_box_factory(BoxFactory factory);
    void clear_box_factory();
    std::string show(bool wait_ready = true, double timeout_sec = 8.0);
    void hide();
    void stop();
    bool is_visible() const { return visible_.load(); }
    bool is_running() const { return running_.load(); }
    std::pair<int, int> screen_size() const;
    void mark_ready_once(const std::string& msg);
    std::vector<EspBox> boxes_for_draw() const;

    void set_fov_circle(float cx, float cy, float radius, float r, float g, float b, float a);
    void set_fov_circle(float cx, float cy, float radius);
    void clear_fov_circle();
    float crosshair_size() const { return crosshair_.size.load(std::memory_order_relaxed); }
    void set_crosshair_size(float v) { crosshair_.size.store(v, std::memory_order_relaxed); }
    float crosshair_thickness() const { return crosshair_.thickness.load(std::memory_order_relaxed); }
    void set_crosshair_thickness(float v) { crosshair_.thickness.store(v, std::memory_order_relaxed); }
    float crosshair_gap() const { return crosshair_.gap.load(std::memory_order_relaxed); }
    void set_crosshair_gap(float v) { crosshair_.gap.store(v, std::memory_order_relaxed); }
    bool crosshair_enabled() const { return crosshair_.enabled.load(std::memory_order_relaxed); }
    void set_crosshair_enabled(bool v) { crosshair_.enabled.store(v, std::memory_order_relaxed); }
    void set_crosshair_color(float r, float g, float b);
    std::tuple<float, float, float> crosshair_color() const;

    void set_draw_interval(int ms) { draw_interval_ms_.store(ms, std::memory_order_relaxed); }
    int draw_interval_ms() const { return draw_interval_ms_.load(std::memory_order_relaxed); }
    bool draw_due() {
        auto now = std::chrono::steady_clock::now();
        if (now - last_draw_tp_ >= std::chrono::milliseconds(draw_interval_ms())) {
            last_draw_tp_ = now;
            return true;
        }
        return false;
    }

    struct CrosshairSettings {
        std::atomic<bool> enabled{false};
        std::atomic<float> size{12.f};
        std::atomic<float> thickness{2.f};
        std::atomic<float> gap{4.f};
        std::atomic<float> r{0.f};
        std::atomic<float> g{1.f};
        std::atomic<float> b{0.f};
    };
    CrosshairSettings crosshair_;

    struct FovCircle {
        std::atomic<float> cx{0.f};
        std::atomic<float> cy{0.f};
        std::atomic<float> radius{0.f};
        std::atomic<float> r{1.f};
        std::atomic<float> g{1.f};
        std::atomic<float> b{1.f};
        std::atomic<float> a{0.06f};
        std::atomic<bool> active{false};
    };
    FovCircle fov_;

private:
    mutable std::mutex factory_lock_;
    BoxFactory box_factory_;
    std::atomic<bool> visible_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> ready_{false};
    std::string ready_msg_;
    std::thread gui_thread_;
    int sw_{1920};
    int sh_{1080};
    GtkLayerCtx layer_{};
    GMainContext* gtk_ctx_{nullptr};
    std::atomic<bool> gtk_ready_{false};

    std::atomic<int> draw_interval_ms_{4};
    std::chrono::steady_clock::time_point last_draw_tp_;

    void mark_ready(const std::string& msg);
    void gui_thread_main();
    bool try_gtk_layershell();
};
