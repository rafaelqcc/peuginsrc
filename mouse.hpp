#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

struct _XDisplay;

class MouseInput {
public:
    MouseInput();
    ~MouseInput();

    bool start();
    void stop();
    bool mouse5_held() const { return aim_hold_.load(std::memory_order_relaxed); }
    bool listener_ok() const { return listener_ok_.load(std::memory_order_acquire); }
    bool has_side_buttons() const { return has_side_buttons_.load(std::memory_order_relaxed); }
    std::string backend() const;
    std::string last_error() const;

    void move_relative(float dx, float dy);

private:
    std::atomic<bool> stop_{false};
    std::atomic<bool> aim_hold_{false};
    std::atomic<bool> use_right_fallback_{false};
    std::atomic<bool> listener_ok_{false};
    std::atomic<bool> has_side_buttons_{false};
    std::thread thread_;
    mutable std::mutex state_mutex_;
    std::condition_variable listen_ready_cv_;
    bool listen_finished_{false};
    std::string backend_;
    std::string last_error_;
    int uinput_fd_{-1};
    bool use_xtest_{false};
    _XDisplay* xdisplay_{nullptr};
    float remain_x_{0}, remain_y_{0};

    void listen_loop();
    bool open_uinput();
    void init_xdisplay();
    void move_xtest(int dx, int dy);
    void set_last_error(std::string msg);
    void signal_listen_ready();
};
