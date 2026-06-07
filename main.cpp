#include "process.hpp"
#include "runtime.hpp"
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#ifdef SOBER_HAS_GUI
#include "gui_app.hpp"
#include <gtk/gtk.h>
#endif

static std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

static void print_help() {
    std::cout << "Usage: sober [options]\n"
              << "  (default)     Kirigami GUI (Plasma-style, like Python sober)\n"
              << "  --menu        Interactive CLI instead of GUI\n"
              << "  -p PID        Attach to process ID\n"
              << "  -n name       Process name hint (default: sober)\n"
              << "  --list-procs  List candidate PIDs\n"
              << "  --help        Show this help\n"
              << "\nCLI commands (with --menu):\n"
              << "  esp on | esp off\n"
              << "  walkspeed on [speed] | walkspeed off\n"
              << "  aimbot on | aimbot off | aimbot <1-100>\n"
              << "  status | help | quit\n";
}

static int run_cli_loop(CheatRuntime& rt) {
    std::cout << "Sober C++ attached. " << rt.status_line() << "\n";
    std::cout << "Type 'help' for commands.\n";

    std::string line;
    while (true) {
        std::cout << ">_ ";
        if (!std::getline(std::cin, line)) break;
        line = trim(line);
        if (line.empty()) continue;
        auto lower = to_lower(line);

        if (lower == "quit" || lower == "exit") break;
        if (lower == "help") {
            print_help();
            continue;
        }
        if (lower == "status") {
            std::cout << rt.status_line() << "\n";
            continue;
        }

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        cmd = to_lower(cmd);

        if (cmd == "esp") {
            std::string sub;
            iss >> sub;
            sub = to_lower(sub);
            if (sub == "on") std::cout << rt.set_esp(true) << "\n";
            else if (sub == "off") std::cout << rt.set_esp(false) << "\n";
            else std::cout << "Usage: esp on | esp off\n";
        } else if (cmd == "walkspeed" || cmd == "ws") {
            std::string sub;
            iss >> sub;
            sub = to_lower(sub);
            if (sub == "on") {
                float sp = 0;
                if (iss >> sp) std::cout << rt.set_walkspeed(true, sp) << "\n";
                else std::cout << rt.set_walkspeed(true) << "\n";
            } else if (sub == "off")
                std::cout << rt.set_walkspeed(false) << "\n";
            else
                std::cout << "Usage: walkspeed on [speed] | walkspeed off\n";
            } else if (cmd == "aimbot" || cmd == "aim") {
                std::string sub;
                iss >> sub;
                sub = to_lower(sub);
                if (sub.empty()) {
                    std::cout << "Usage: aimbot on | off | <speed>\n";
                    continue;
                }
                if (sub == "on" || sub == "off") {
                    std::cout << rt.set_aimbot(sub == "on") << "\n";
                } else {
                    try {
                        float sp = std::stof(sub);
                        std::cout << rt.set_aimbot_speed(sp) << "\n";
                    } catch (...) {
                        std::cout << "Invalid aimbot speed\n";
                    }
                }
            } else {
            std::cout << "Unknown command. Type 'help'.\n";
        }
    }

    rt.shutdown();
    std::cout << "Bye.\n";
    return 0;
}

int main(int argc, char** argv) {
    if (std::getenv("DISPLAY")) XInitThreads();

    std::optional<int> pid;
    std::string name_hint = "sober";
    bool use_menu = false;
    bool list_procs = false;
    bool show_help = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--list-procs") list_procs = true;
        else if (arg == "--menu") use_menu = true;
        else if (arg == "-p" && i + 1 < argc) pid = std::stoi(argv[++i]);
        else if (arg == "-n" && i + 1 < argc) name_hint = argv[++i];
        else if (arg == "--help" || arg == "-h") show_help = true;
    }

    if (show_help) {
        print_help();
        return 0;
    }

    if (list_procs) {
        for (const auto& [p, comm, label, base] : list_apk_processes())
            std::cout << p << "  " << comm << "  0x" << std::hex << base << std::dec << "  "
                      << label << "\n";
        return 0;
    }

    try {
        auto session = connect_session(pid, name_hint);
        CheatRuntime rt(std::move(session));
        rt.start_workers();

#ifndef SOBER_HAS_GUI
        if (!use_menu) {
            std::cerr << "GUI not available (rebuild with Qt6 + Kirigami). Using CLI.\n";
            use_menu = true;
        }
        return run_cli_loop(rt);
#else
        if (!use_menu) {
            if ((std::getenv("WAYLAND_DISPLAY") || std::getenv("DISPLAY")) && !gtk_init_check())
                std::cerr << "Warning: gtk_init_check() failed — overlay may not work\n";
            return run_sober_gui(argc, argv, rt);
        }
        if ((std::getenv("WAYLAND_DISPLAY") || std::getenv("DISPLAY")) && !gtk_init_check())
            std::cerr << "Warning: gtk_init_check() failed — overlay may not work\n";
        return run_cli_loop(rt);
#endif
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
