#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

struct Vec3 {
    float x{}, y{}, z{};
};

struct EspBox {
    float x1{}, y1{}, x2{}, y2{};
    float r{1.f}, g{1.f}, b{1.f};
    bool fill{false};
    float a{1.f};
    std::string name;
};

class MemError : public std::runtime_error {
public:
    explicit MemError(const std::string& msg) : std::runtime_error(msg) {}
};

inline bool is_finite_vec(float x, float y, float z) {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

inline bool vec3_sane(const Vec3& v) {
    if (!is_finite_vec(v.x, v.y, v.z)) return false;
    float m = std::max(std::abs(v.x), std::max(std::abs(v.y), std::abs(v.z)));
    if (m > 1e6f) return false;
    return std::abs(v.x) + std::abs(v.y) + std::abs(v.z) > 0.05f;
}
