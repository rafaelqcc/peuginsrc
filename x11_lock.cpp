#include "x11_lock.hpp"

std::mutex& x11_mutex() {
    static std::mutex m;
    return m;
}
