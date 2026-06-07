#include "process.hpp"
#include "game.hpp"
#include "offsets.hpp"
#include "x11_lock.hpp"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_set>

static bool is_base_apk(const std::string& path) {
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower.find("base.apk") != std::string::npos;
}

std::vector<MapRow> parse_maps(int pid) {
    std::vector<MapRow> rows;
    std::ifstream f("/proc/" + std::to_string(pid) + "/maps");
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string range, perms, off_str;
        if (!(iss >> range >> perms >> off_str)) continue;
        auto dash = range.find('-');
        if (dash == std::string::npos) continue;
        MapRow r;
        r.lo = std::stoull(range.substr(0, dash), nullptr, 16);
        r.hi = std::stoull(range.substr(dash + 1), nullptr, 16);
        r.file_off = std::stoull(off_str, nullptr, 16);
        r.perms = perms;
        std::string rest;
        std::getline(iss, rest);
        auto start = rest.find_first_not_of(" \t");
        if (start != std::string::npos) {
            std::istringstream rs(rest.substr(start));
            std::string dev, inode;
            rs >> dev >> inode;
            std::string path;
            std::getline(rs, path);
            auto ps = path.find_first_not_of(" \t");
            r.path = ps != std::string::npos ? path.substr(ps) : "";
        }
        rows.push_back(r);
    }
    return rows;
}

bool has_base_apk(int pid) {
    for (const auto& r : parse_maps(pid))
        if (is_base_apk(r.path)) return true;
    return false;
}

std::string proc_comm(int pid) {
    std::ifstream f("/proc/" + std::to_string(pid) + "/comm");
    std::string s;
    std::getline(f, s);
    while (!s.empty() && (s.back() == '\0' || s.back() == '\n')) s.pop_back();
    return s.empty() ? "?" : s;
}

static std::vector<std::tuple<std::string, std::uint64_t, std::uint64_t>> roblox_module_bases(int pid) {
    std::vector<std::tuple<std::string, std::uint64_t, std::uint64_t>> out;
    if (!has_base_apk(pid)) return out;
    auto rows = parse_maps(pid);

    std::vector<std::pair<std::uint64_t, std::uint64_t>> memfd_rx;
    for (const auto& r : rows) {
        if (r.file_off != 0 || r.perms.find("r-x") == std::string::npos) continue;
        std::string pl = r.path;
        std::transform(pl.begin(), pl.end(), pl.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (pl.find("kwin") != std::string::npos) continue;
        if (pl.find("memfd") != std::string::npos || r.path.rfind("/memfd", 0) == 0)
            memfd_rx.emplace_back(r.hi - r.lo, r.lo);
    }
    if (!memfd_rx.empty()) {
        auto best = *std::max_element(memfd_rx.begin(), memfd_rx.end());
        out.emplace_back("memfd r-xp (Roblox image)", best.second, best.first);
    }

    for (const auto& r : rows) {
        if (!is_base_apk(r.path)) continue;
        std::uint64_t image = r.lo - r.file_off;
        if (image > 0)
            out.emplace_back("base.apk image (vma-off)", image, r.hi - r.lo);
        if (r.file_off == 0 && r.perms.find("r-x") != std::string::npos)
            out.emplace_back("base.apk r-xp", r.lo, r.hi - r.lo);
    }
    return out;
}

std::vector<std::tuple<int, std::string, std::string, std::uint64_t>> list_apk_processes() {
    std::vector<std::tuple<int, std::string, std::string, std::uint64_t>> rows;
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (name.empty() || !std::isdigit(name[0])) continue;
        int pid = std::stoi(name);
        if (!has_base_apk(pid)) continue;
        auto bases = roblox_module_bases(pid);
        if (bases.empty())
            rows.emplace_back(pid, proc_comm(pid), "base.apk (no base)", 0);
        else
            for (const auto& [label, base, _] : bases)
                rows.emplace_back(pid, proc_comm(pid), label, base);
    }
    return rows;
}

int find_pid(const std::string& name_hint) {
    std::string hint = name_hint;
    std::transform(hint.begin(), hint.end(), hint.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::vector<std::pair<int, int>> ranked;
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (name.empty() || !std::isdigit(name[0])) continue;
        int pid = std::stoi(name);
        if (!has_base_apk(pid)) continue;
        auto bases = roblox_module_bases(pid);
        if (bases.empty()) continue;
        int score = static_cast<int>(std::get<2>(bases[0]) / (1024 * 1024));
        std::string comm = proc_comm(pid);
        std::string cl = comm;
        std::transform(cl.begin(), cl.end(), cl.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (cl == "main") score += 50000;
        if (cl.find("sober") != std::string::npos) score += 10000;
        if (!hint.empty() && cl.find(hint) != std::string::npos) score += 5000;
        ranked.emplace_back(score, pid);
    }
    if (ranked.empty())
        throw MemError("No process has base.apk mapped. Join a game in Sober first.");
    std::sort(ranked.begin(), ranked.end(), std::greater<>());
    return ranked.front().second;
}

std::vector<std::tuple<std::string, std::uint64_t>> candidate_bases(int pid) {
    std::vector<std::tuple<std::string, std::uint64_t>> out;
    for (const auto& [label, base, _] : roblox_module_bases(pid))
        out.emplace_back(label, base);
    if (!out.empty()) return out;

    const std::uint64_t need = std::max(offsets::VISUAL_ENGINE_POINTER, offsets::FAKE_DATAMODEL_POINTER) + 8;
    std::vector<std::tuple<std::string, std::uint64_t, int>> fallback;
    std::unordered_set<std::uint64_t> seen;
    for (const auto& r : parse_maps(pid)) {
        std::uint64_t size = r.hi - r.lo;
        if (size < need || seen.count(r.lo)) continue;
        seen.insert(r.lo);
        int prio = r.path.find("mimalloc") != std::string::npos ? 0 : 2;
        fallback.emplace_back(r.path.empty() ? "[anon]" : r.path, r.lo, prio);
    }
    std::sort(fallback.begin(), fallback.end(),
              [](const auto& a, const auto& b) {
                  if (std::get<2>(a) != std::get<2>(b)) return std::get<2>(a) < std::get<2>(b);
                  return std::get<1>(a) > std::get<1>(b);
              });
    for (const auto& [tag, lo, _] : fallback) out.emplace_back(tag, lo);
    return out;
}

std::pair<std::uint64_t, std::string> resolve_base(Memory& mem) {
    for (const auto& [tag, base] : candidate_bases(mem.pid())) {
        if (try_datamodel_visual(mem, base) || try_datamodel_fake(mem, base))
            return {base, tag};
    }
    throw MemError("could not find module base (no valid pointer chain)");
}

GameSession::GameSession(int pid, std::uint64_t base, std::string tag)
    : pid(pid), mem(pid), base(base), base_tag(std::move(tag)) {}

GameSession connect_session(std::optional<int> pid, const std::string& name_hint) {
    int p = pid.value_or(find_pid(name_hint));
    Memory mem(p);
    auto [base, tag] = resolve_base(mem);
    return GameSession(p, base, tag);
}

std::optional<std::tuple<int, int, int, int>> find_game_window_rect(int pid) {
    if (!std::getenv("DISPLAY")) return std::nullopt;

    struct Cache {
        std::mutex mu;
        int pid{-1};
        std::optional<std::tuple<int, int, int, int>> rect;
        std::chrono::steady_clock::time_point at{};
    };
    static Cache cache;
    constexpr auto ttl = std::chrono::milliseconds(250);
    const auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(cache.mu);
        if (cache.pid == pid && cache.rect && (now - cache.at) < ttl) return *cache.rect;
    }

    std::lock_guard xlock(x11_mutex());
    Display* d = XOpenDisplay(nullptr);
    if (!d) return std::nullopt;

    Window root = DefaultRootWindow(d);
    Atom clients = XInternAtom(d, "_NET_CLIENT_LIST", False);
    Atom wm_pid = XInternAtom(d, "_NET_WM_PID", False);
    Atom actual{};
    int fmt = 0;
    unsigned long n = 0, after = 0;
    unsigned char* data = nullptr;
    std::optional<std::tuple<int, int, int, int, int>> best;

    const int prop_ok =
        XGetWindowProperty(d, root, clients, 0, 8192, False, XA_WINDOW, &actual, &fmt, &n,
                           &after, &data);
    if (prop_ok == Success && data && fmt == 32 && n >= sizeof(Window)) {
        const int count = static_cast<int>(n / sizeof(Window));
        auto* wins = reinterpret_cast<Window*>(data);
        for (int i = 0; i < count; ++i) {
            const Window w = wins[i];
            unsigned char* pd = nullptr;
            if (XGetWindowProperty(d, w, wm_pid, 0, 1, False, XA_CARDINAL, &actual, &fmt, &n,
                                   &after, &pd) != Success || !pd || fmt != 32 || n < 4) {
                if (pd) XFree(pd);
                continue;
            }
            const unsigned long wp = *reinterpret_cast<unsigned long*>(pd);
            XFree(pd);
            if (static_cast<int>(wp) != pid) continue;
            XWindowAttributes attr{};
            if (!XGetWindowAttributes(d, w, &attr)) continue;
            int rx = 0, ry = 0;
            Window child{};
            XTranslateCoordinates(d, w, root, 0, 0, &rx, &ry, &child);
            const int area = attr.width * attr.height;
            if (area < 64 * 64) continue;
            if (!best || area > std::get<4>(*best))
                best = {rx, ry, attr.width, attr.height, area};
        }
        XFree(data);
    } else if (data) {
        XFree(data);
    }

    XCloseDisplay(d);
    std::optional<std::tuple<int, int, int, int>> out;
    if (best)
        out = std::make_tuple(std::get<0>(*best), std::get<1>(*best), std::get<2>(*best),
                              std::get<3>(*best));

    std::lock_guard lock(cache.mu);
    cache.pid = pid;
    cache.rect = out;
    cache.at = now;
    return out;
}
