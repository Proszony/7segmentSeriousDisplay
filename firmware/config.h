#pragma once

#include <string>

#define MAX_CHIPS      6
#define MAX_MODULES    8

struct Params {
    std::string port = "/dev/serial0";
    int baud = 115200;
    int chips = 1;
    int modules_x = 1;
    int modules_y = 1;
    int cs_pins[MAX_MODULES] = {25, 26, 27, 22, 23, 24, 17, 18};
};

void config_load(Params &p, const std::string &path);
void config_set_cs_pins(Params &p, const std::string &s);
