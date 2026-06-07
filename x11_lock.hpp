#pragma once
#include <mutex>

/** Serialize all Xlib calls (ESP window probe, XTest, cursor). */
std::mutex& x11_mutex();
