#include "config.h"

#include <fstream>
#include <iostream>
#include <string>

static std::string trim(const std::string &s)
{
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

void config_set_cs_pins(Params &p, const std::string &s)
{
    std::string rest = s;
    int idx = 0;
    size_t pos;
    while ((pos = rest.find(',')) != std::string::npos) {
        p.cs_pins[idx++] = std::stoi(trim(rest.substr(0, pos)));
        rest.erase(0, pos + 1);
    }
    p.cs_pins[idx++] = std::stoi(trim(rest));
}

void config_load(Params &p, const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cout << "Config: no " << path << ", using defaults\n";
        return;
    }

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            std::cerr << "Config: skipping malformed line: " << line << "\n";
            continue;
        }

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "port")           p.port = val;
        else if (key == "baud")      p.baud = std::stoi(val);
        else if (key == "chips")     p.chips = std::stoi(val);
        else if (key == "cs-pins")   config_set_cs_pins(p, val);
        else if (key == "mod-x")     p.modules_x = std::stoi(val);
        else if (key == "mod-y")     p.modules_y = std::stoi(val);
        else if (key == "refresh-ms") p.refresh_ms = std::stoi(val);
        else if (key == "reinit-interval") p.reinit_interval = std::stoi(val);
        else                         std::cerr << "Config: unknown key: " << key << "\n";
    }
}
