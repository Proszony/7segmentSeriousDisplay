#include "config.h"
#include "test_util.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

std::string temp_path(const char *suffix)
{
    return "/tmp/7seg-config-" + std::to_string(getpid()) + "-" + suffix;
}

void write_file(const std::string &path, const std::string &content)
{
    std::ofstream f(path);
    f << content;
}

void test_defaults_when_missing()
{
    TEST_SECTION("missing config keeps defaults");

    Params p;
    config_load(p, temp_path("missing"));

    CHECK_EQ(p.port, std::string("/dev/serial0"));
    CHECK_EQ(p.baud, 115200);
    CHECK_EQ(p.chips, 1);
    CHECK_EQ(p.modules_x, 1);
    CHECK_EQ(p.modules_y, 1);
    CHECK_EQ(p.refresh_ms, 10);
    CHECK_EQ(p.cs_pins[0], 25);
}

void test_parses_all_keys()
{
    TEST_SECTION("all known keys are parsed");

    const std::string path = temp_path("full");
    write_file(path,
               "# full config\n"
               "\n"
               "port = /dev/ttyUSB7\n"
               "baud=921600\n"
               "chips = 4\n"
               "cs-pins = 4, 17 , 27\n"
               "mod-x = 2\n"
               "mod-y = 3\n"
               "refresh-ms = 5\n"
               "reinit-interval = 1.5\n");

    Params p;
    config_load(p, path);

    CHECK_EQ(p.port, std::string("/dev/ttyUSB7"));
    CHECK_EQ(p.baud, 921600);
    CHECK_EQ(p.chips, 4);
    CHECK_EQ(p.cs_pins[0], 4);
    CHECK_EQ(p.cs_pins[1], 17);
    CHECK_EQ(p.cs_pins[2], 27);
    CHECK_EQ(p.modules_x, 2);
    CHECK_EQ(p.modules_y, 3);
    CHECK_EQ(p.refresh_ms, 5);
    CHECK(p.reinit_interval > 1.49 && p.reinit_interval < 1.51);

    std::remove(path.c_str());
}

void test_ignores_comments_and_malformed()
{
    TEST_SECTION("comments, blank lines and malformed lines are ignored");

    const std::string path = temp_path("malformed");
    write_file(path,
               "# comment\n"
               "   \n"
               "this line has no equals sign\n"
               "unknown-key = 42\n"
               "baud = 57600\n");

    Params p;
    config_load(p, path);

    CHECK_EQ(p.baud, 57600);
    CHECK_EQ(p.port, std::string("/dev/serial0"));

    std::remove(path.c_str());
}

void test_cs_pins_defaults_preserved()
{
    TEST_SECTION("cs-pins only overwrite provided entries");

    const std::string path = temp_path("cspins");
    write_file(path, "cs-pins = 10,11\n");

    Params p;
    config_load(p, path);

    CHECK_EQ(p.cs_pins[0], 10);
    CHECK_EQ(p.cs_pins[1], 11);
    CHECK_EQ(p.cs_pins[2], 27); // default retained

    std::remove(path.c_str());
}

void test_bad_number_throws()
{
    TEST_SECTION("non-numeric value is reported by throwing");

    const std::string path = temp_path("badnum");
    write_file(path, "baud = fast\n");

    Params p;
    CHECK_THROWS(config_load(p, path), std::invalid_argument);

    std::remove(path.c_str());
}

}

int main()
{
    test_defaults_when_missing();
    test_parses_all_keys();
    test_ignores_comments_and_malformed();
    test_cs_pins_defaults_preserved();
    test_bad_number_throws();
    return test::summary();
}
