#include "mouse.hpp"
#include "x11_lock.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

// BTN_SIDE, BTN_EXTRA (mouse5), BTN_FORWARD, BTN_RIGHT fallback, BTN_LEFT
static const int MOUSE5_CODES[] = {275, 276, 277};
static const int AIM_HOLD_FALLBACK = 273;
static bool is_virtual_aim_device(libevdev* dev) {
    const char* name = libevdev_get_name(dev);
    return name && std::strstr(name, "sober-aim") != nullptr;
}

static bool has_side_button(libevdev* dev) {
    for (int code : MOUSE5_CODES) {
        if (libevdev_has_event_code(dev, EV_KEY, code)) return true;
    }
    return false;
}

static bool has_right_button(libevdev* dev) {
    return libevdev_has_event_code(dev, EV_KEY, AIM_HOLD_FALLBACK);
}

static bool should_listen_device(libevdev* dev) {
    if (is_virtual_aim_device(dev)) return false;
    if (!libevdev_has_event_type(dev, EV_KEY)) return false;
    return has_side_button(dev) || has_right_button(dev);
}

static bool is_aim_key_event(int code, bool right_fallback) {
    for (int c : MOUSE5_CODES) {
        if (code == c) return true;
    }
    return right_fallback && code == AIM_HOLD_FALLBACK;
}

static void drain_sync_events(libevdev* dev, bool right_fallback) {
    input_event ev{};
    while (libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev) == LIBEVDEV_READ_STATUS_SYNC) {
        if (ev.type != EV_KEY) continue;
        if (!is_aim_key_event(ev.code, right_fallback)) continue;
        // state-only sync; do not set aim_hold_ from historical sync
        (void)ev;
    }
}

MouseInput::MouseInput() = default;

MouseInput::~MouseInput() { stop(); }

void MouseInput::set_last_error(std::string msg) {
    std::lock_guard lock(state_mutex_);
    last_error_ = std::move(msg);
}

std::string MouseInput::last_error() const {
    std::lock_guard lock(state_mutex_);
    return last_error_;
}

std::string MouseInput::backend() const {
    std::lock_guard lock(state_mutex_);
    return backend_;
}

void MouseInput::signal_listen_ready() {
    {
        std::lock_guard lock(state_mutex_);
        listen_finished_ = true;
    }
    listen_ready_cv_.notify_one();
}

void MouseInput::init_xdisplay() {
    if (!std::getenv("DISPLAY")) return;
    std::lock_guard lock(x11_mutex());
    if (xdisplay_) return;
    xdisplay_ = XOpenDisplay(nullptr);
    if (xdisplay_) use_xtest_ = true;
}

bool MouseInput::open_uinput() {
    uinput_fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd_ < 0) return false;

    ioctl(uinput_fd_, UI_SET_EVBIT, EV_REL);
    ioctl(uinput_fd_, UI_SET_RELBIT, REL_X);
    ioctl(uinput_fd_, UI_SET_RELBIT, REL_Y);
    ioctl(uinput_fd_, UI_SET_EVBIT, EV_KEY);
    ioctl(uinput_fd_, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(uinput_fd_, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(uinput_fd_, UI_SET_KEYBIT, BTN_MIDDLE);

    uinput_user_dev uidev{};
    std::strncpy(uidev.name, "sober-aim-virtual-mouse", UINPUT_MAX_NAME_SIZE - 1);
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x1234;
    uidev.id.product = 0x5678;
    uidev.id.version = 1;
    if (write(uinput_fd_, &uidev, sizeof(uidev)) < 0 ||
        ioctl(uinput_fd_, UI_DEV_CREATE) < 0) {
        close(uinput_fd_);
        uinput_fd_ = -1;
        return false;
    }
    return true;
}

bool MouseInput::start() {
    if (thread_.joinable()) {
        stop_ = true;
        thread_.join();
    }

    stop_ = false;
    listener_ok_.store(false, std::memory_order_relaxed);
    has_side_buttons_.store(false, std::memory_order_relaxed);
    use_right_fallback_.store(false, std::memory_order_relaxed);
    aim_hold_.store(false, std::memory_order_relaxed);
    {
        std::lock_guard lock(state_mutex_);
        last_error_.clear();
        backend_.clear();
        listen_finished_ = false;
    }

    init_xdisplay();
    if (open_uinput()) {
        std::lock_guard lock(state_mutex_);
        backend_ = "uinput";
    }
    if (use_xtest_) {
        std::lock_guard lock(state_mutex_);
        if (!backend_.empty()) backend_ += "+xtest";
        else backend_ = "xtest";
    }
    if (backend().empty()) {
        set_last_error("need uinput (/dev/uinput) or DISPLAY for XTest");
        return false;
    }

    thread_ = std::thread(&MouseInput::listen_loop, this);

    {
        std::unique_lock lock(state_mutex_);
        listen_ready_cv_.wait_for(lock, std::chrono::milliseconds(500),
                                  [this] { return listen_finished_; });
    }

    if (!listener_ok_.load(std::memory_order_acquire)) {
        use_right_fallback_.store(true, std::memory_order_relaxed);
        if (!last_error().empty() && use_xtest_) set_last_error("");
        else if (!use_xtest_) return false;
    }
    return true;
}

void MouseInput::stop() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    if (uinput_fd_ >= 0) {
        ioctl(uinput_fd_, UI_DEV_DESTROY);
        close(uinput_fd_);
        uinput_fd_ = -1;
    }
    {
        std::lock_guard lock(x11_mutex());
        if (xdisplay_) {
            XCloseDisplay(xdisplay_);
            xdisplay_ = nullptr;
        }
    }
    use_xtest_ = false;
    aim_hold_.store(false, std::memory_order_relaxed);
}

void MouseInput::move_xtest(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    std::lock_guard lock(x11_mutex());
    if (!xdisplay_) return;
    // Relative motion only — XQueryPointer on XWayland root can SIGSEGV.
    XTestFakeRelativeMotionEvent(xdisplay_, dx, dy, CurrentTime);
    XSync(xdisplay_, False);
}

void MouseInput::move_relative(float dx, float dy) {
    if (dx == 0.f && dy == 0.f) return;
    remain_x_ += dx;
    remain_y_ += dy;
    int mx = static_cast<int>(remain_x_);
    int my = static_cast<int>(remain_y_);
    if (mx == 0 && my == 0) return;
    remain_x_ -= mx;
    remain_y_ -= my;

    // Prefer uinput (works on native Wayland); XTest is fallback only.
    if (uinput_fd_ >= 0) {
        input_event ev{};
        ev.type = EV_REL;
        ev.code = REL_X;
        ev.value = mx;
        write(uinput_fd_, &ev, sizeof(ev));
        ev.code = REL_Y;
        ev.value = my;
        write(uinput_fd_, &ev, sizeof(ev));
        ev.type = EV_SYN;
        ev.code = SYN_REPORT;
        ev.value = 0;
        write(uinput_fd_, &ev, sizeof(ev));
    } else if (use_xtest_) {
        move_xtest(mx, my);
    }
}

struct EvDevHandle {
    libevdev* dev{};
    int fd{-1};
    bool side_buttons{false};
};

void MouseInput::listen_loop() {
    std::vector<EvDevHandle> handles;
    bool any_side = false;
    DIR* dir = opendir("/dev/input");
    if (!dir) {
        set_last_error("cannot open /dev/input");
        signal_listen_ready();
        return;
    }

    struct dirent* ent;
    while ((ent = readdir(dir))) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;
        std::string path = std::string("/dev/input/") + ent->d_name;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        libevdev* dev = nullptr;
        if (libevdev_new_from_fd(fd, &dev) < 0) {
            close(fd);
            continue;
        }
        if (!should_listen_device(dev)) {
            libevdev_free(dev);
            close(fd);
            continue;
        }
        const bool side = has_side_button(dev);
        any_side = any_side || side;
        handles.push_back({dev, fd, side});
    }
    closedir(dir);

    if (handles.empty()) {
        set_last_error("no mouse devices with side/right buttons (check input group)");
        signal_listen_ready();
        return;
    }

    has_side_buttons_.store(any_side, std::memory_order_relaxed);
    if (!any_side) use_right_fallback_.store(true, std::memory_order_relaxed);

    listener_ok_.store(true, std::memory_order_release);
    signal_listen_ready();

    std::vector<pollfd> pfds;
    pfds.reserve(handles.size());
    for (const auto& h : handles) pfds.push_back({h.fd, POLLIN, 0});

    while (!stop_.load(std::memory_order_relaxed)) {
        const int polled = poll(pfds.data(), static_cast<nfds_t>(pfds.size()), 50);
        if (polled <= 0) continue;

        const bool right_fallback = use_right_fallback_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < handles.size(); ++i) {
            const short rev = pfds[i].revents;
            if (rev & (POLLERR | POLLHUP | POLLNVAL)) continue;
            if (!(rev & POLLIN)) continue;

            input_event ev{};
            while (true) {
                int rc = libevdev_next_event(handles[i].dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
                if (rc == LIBEVDEV_READ_STATUS_SYNC) {
                    drain_sync_events(handles[i].dev, right_fallback);
                    continue;
                }
                if (rc != LIBEVDEV_READ_STATUS_SUCCESS) break;
                if (ev.type != EV_KEY) continue;
                if (!is_aim_key_event(ev.code, right_fallback)) continue;
                aim_hold_.store(ev.value != 0, std::memory_order_relaxed);
            }
        }
    }

    for (auto& h : handles) {
        libevdev_free(h.dev);
        close(h.fd);
    }
}
