#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <algorithm>
#include <chrono>
#include <linux/spi/spidev.h>
#include <gpiod.h>

#include "config.h"
#include "framing.h"
#include "uart.h"

#define REG_DIGIT0     0x01
#define REG_DIGIT1     0x02
#define REG_DIGIT2     0x03
#define REG_DIGIT3     0x04
#define REG_DIGIT4     0x05
#define REG_DIGIT5     0x06
#define REG_DIGIT6     0x07
#define REG_DIGIT7     0x08
#define REG_DECODE     0x09
#define REG_INTENSITY  0x0A
#define REG_SCANLIMIT  0x0B
#define REG_SHUTDOWN   0x0C
#define REG_DISPTEST   0x0F

#define MODULE_W       8

static const uint32_t SPI_SPEED = 1000000;

// ---- CLI Params ----

static void print_help(void)
{
    std::cout << "Usage: display_fw [options]\n";
    std::cout << "  --port <dev>     UART device (default: /dev/serial0)\n";
    std::cout << "  --baud <rate>    UART baudrate (default: 115200)\n";
    std::cout << "  --chips <n>      MAX7219 per module in daisy chain (default: 1)\n";
    std::cout << "  --cs-pins <list> CS GPIO pins per module (comma-separated, default: 25,26,27,22,23,24,17,18)\n";
    std::cout << "  --mod-x <n>      Modules in X direction (default: 1)\n";
    std::cout << "  --mod-y <n>      Modules in Y direction (default: 1)\n";
    std::cout << "  --config <path>  Config file path (default: ./config.cfg)\n";
    std::cout << "  --help           This message\n";
}

static void parse_args(int argc, char **argv, Params &p, std::string &config_path)
{
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) p.port = argv[++i];
        else if (arg == "--baud" && i + 1 < argc) p.baud = std::stoi(argv[++i]);
        else if (arg == "--chips" && i + 1 < argc) p.chips = std::stoi(argv[++i]);
        else if (arg == "--cs-pins" && i + 1 < argc) config_set_cs_pins(p, argv[++i]);
        else if (arg == "--mod-x" && i + 1 < argc) p.modules_x = std::stoi(argv[++i]);
        else if (arg == "--mod-y" && i + 1 < argc) p.modules_y = std::stoi(argv[++i]);
        else if (arg == "--config" && i + 1 < argc) config_path = argv[++i];
        else if (arg == "--help") { print_help(); exit(0); }
    }
    if (p.chips < 1 || p.chips > MAX_CHIPS) {
        std::cerr << "chips must be 1.." << MAX_CHIPS << "\n";
        exit(1);
    }
    int total_mod = p.modules_x * p.modules_y;
    if (total_mod > MAX_MODULES) {
        std::cerr << "total modules (" << total_mod << ") exceeds MAX_MODULES (" << MAX_MODULES << ")\n";
        exit(1);
    }
}

// ---- Bit translation ----

static uint8_t pc_to_max7219(uint8_t pc)
{
    uint8_t m = 0;
    for (int i = 0; i < 7; i++)
        if (pc & (1u << i))
            m |= (1u << (6 - i));
    return m;
}

// ---- SPI + GPIO CS (libgpiod) ----

static int spi_fd = -1;
struct gpiod_chip *gpio_chip = NULL;

static bool spi_init(void)
{
    spi_fd = open("/dev/spidev0.0", O_RDWR);
    if (spi_fd < 0) {
        std::cerr << "SPI: cannot open /dev/spidev0.0: " << strerror(errno) << "\n";
        return false;
    }

    uint8_t mode = SPI_MODE_0 | SPI_NO_CS;
    uint8_t bits = 8;
    uint32_t speed = SPI_SPEED;

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        std::cerr << "SPI: config failed: " << strerror(errno) << "\n";
        close(spi_fd);
        spi_fd = -1;
        return false;
    }
    return true;
}

static bool gpio_init(void)
{
    gpio_chip = gpiod_chip_open_by_number(0);
    if (!gpio_chip) {
        std::cerr << "GPIO: chip open failed: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

static void gpio_cleanup(void)
{
    if (gpio_chip) { gpiod_chip_close(gpio_chip); gpio_chip = NULL; }
}

static void spi_cleanup(void)
{
    if (spi_fd >= 0) { close(spi_fd); spi_fd = -1; }
}

static void spi_tx(const uint8_t *buf, int len)
{
    struct spi_ioc_transfer tr = {};
    tr.tx_buf = (unsigned long)buf;
    tr.len = len;
    tr.speed_hz = SPI_SPEED;
    tr.bits_per_word = 8;
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0)
        std::cerr << "SPI tx error: " << strerror(errno) << "\n";
}

// ---- Module (MAX7219 chips in daisy chain, each module has own CS) ----

static const uint8_t init_cmds[][2] = {
    {REG_SHUTDOWN, 0x01},
    {REG_DECODE, 0x00},
    {REG_SCANLIMIT, 0x07},
    {REG_INTENSITY, 0x08},
    {REG_DISPTEST, 0x00},
};

struct Module {
    int chips;
    struct gpiod_line *cs_line;
    uint8_t fb[MAX_CHIPS][8];

    Module() : chips(0), cs_line(nullptr) { memset(fb, 0, sizeof(fb)); }

    void init(int n_chips, int cs_pin)
    {
        chips = n_chips;
        memset(fb, 0, sizeof(fb));

        cs_line = gpiod_chip_get_line(gpio_chip, cs_pin);
        if (!cs_line) {
            std::cerr << "Module: get CS pin " << cs_pin << " failed: " << strerror(errno) << "\n";
            return;
        }
        if (gpiod_line_request_output(cs_line, "max7219", 1) < 0) {
            std::cerr << "Module: request CS pin " << cs_pin << " failed: " << strerror(errno) << "\n";
            return;
        }

        for (auto &c : init_cmds) {
            uint8_t buf[MAX_CHIPS * 2];
            for (int ci = 0; ci < chips; ci++) {
                int chip = chips - 1 - ci;
                buf[ci * 2 + 0] = c[0];
                buf[ci * 2 + 1] = c[1];
            }
            gpiod_line_set_value(cs_line, 0);
            spi_tx(buf, chips * 2);
            gpiod_line_set_value(cs_line, 1);
            usleep(10);
        }
    }

    void set_cell(int x, int y, uint8_t max_val)
    {
        if (x < 0 || x >= MODULE_W || y < 0 || y >= chips) return;
        int d = 7 - x;
        fb[y][d] = max_val;
    }

    void refresh(void)
    {
        if (!cs_line) return;
        for (int d = 0; d < 8; d++) {
            uint8_t buf[MAX_CHIPS * 2];
            for (int ci = 0; ci < chips; ci++) {
                int c = chips - 1 - ci;
                buf[ci * 2 + 0] = REG_DIGIT0 + d;
                buf[ci * 2 + 1] = fb[c][d];
            }
            gpiod_line_set_value(cs_line, 0);
            spi_tx(buf, chips * 2);
            gpiod_line_set_value(cs_line, 1);
            usleep(5);
        }
    }

    void clear(void)
    {
        if (!cs_line) return;
        for (int d = 0; d < 8; d++) {
            uint8_t buf[MAX_CHIPS * 2];
            for (int ci = 0; ci < chips; ci++) {
                int c = chips - 1 - ci;
                fb[c][d] = 0;
                buf[ci * 2 + 0] = REG_DIGIT0 + d;
                buf[ci * 2 + 1] = 0x00;
            }
            gpiod_line_set_value(cs_line, 0);
            spi_tx(buf, chips * 2);
            gpiod_line_set_value(cs_line, 1);
            usleep(5);
        }
    }

    void release_cs(void)
    {
        if (cs_line) {
            gpiod_line_set_value(cs_line, 1);
            gpiod_line_release(cs_line);
            cs_line = nullptr;
        }
    }
};

// ---- Display (grid of modules) ----

struct Display {
    Module *modules;
    int mx, my, mw, mh;

    Display() : modules(nullptr), mx(0), my(0), mw(MODULE_W), mh(1) {}
    ~Display() { delete[] modules; }

    bool init(int mod_x, int mod_y, int mod_w, int mod_h, const int *cs_pins)
    {
        mx = mod_x;
        my = mod_y;
        mw = mod_w;
        mh = mod_h;

        int total = mx * my;
        if (total < 1) return false;

        modules = new Module[total];
        for (int i = 0; i < total; i++)
            modules[i].init(mh, cs_pins[i]);

        return true;
    }

    void release_cs(void)
    {
        int total = mx * my;
        for (int i = 0; i < total; i++)
            modules[i].release_cs();
    }

    void set_frame(Frame &f)
    {
        for (int myi = 0; myi < my; myi++) {
            for (int mxi = 0; mxi < mx; mxi++) {
                int ox = mxi * mw;
                int oy = myi * mh;
                int idx = myi * mx + mxi;

                for (int y = 0; y < mh; y++) {
                    for (int x = 0; x < mw; x++) {
                        int gx = ox + x;
                        int gy = oy + y;
                        if (gx < f.max_X && gy < f.max_Y) {
                            uint8_t pc = f.cells[gy * f.max_X + gx];
                            modules[idx].set_cell(x, y, pc_to_max7219(pc));
                        }
                    }
                }
            }
        }
    }

    void refresh(void)
    {
        for (int i = 0; i < mx * my; i++)
            modules[i].refresh();
    }

    void clear(void)
    {
        for (int i = 0; i < mx * my; i++)
            modules[i].clear();
    }
};

// ---- Main ----

int main(int argc, char **argv)
{
    Params p;
    std::string config_path = "./config.cfg";
    parse_args(argc, argv, p, config_path);
    config_load(p, config_path);
    parse_args(argc, argv, p, config_path);

    std::cout << "Display Firmware\n";
    std::cout << "================\n";
    std::cout << "UART: " << p.port << " @ " << p.baud << " baud\n";
    std::cout << "SPI:  /dev/spidev0.0 @ 1 MHz\n";
    std::cout << "CS:   ";
    for (int i = 0; i < p.modules_x * p.modules_y; i++)
        std::cout << "mod" << i << "=GPIO" << p.cs_pins[i] << " ";
    std::cout << "\n";
    std::cout << "Display: " << (p.modules_x * p.modules_y) << " module(s)"
              << " (" << p.modules_x << "x" << p.modules_y << ")"
              << ", " << p.chips << " chip(s) per module"
              << " = " << (p.modules_x * p.modules_y * p.chips * 8) << " cells\n";
    std::cout << "Press Ctrl+C to exit.\n\n";

    if (!spi_init()) { return 1; }
    if (!gpio_init()) { spi_cleanup(); return 1; }

    Display display;
    if (!display.init(p.modules_x, p.modules_y, MODULE_W, p.chips, p.cs_pins)) {
        std::cerr << "Display init failed\n";
        display.release_cs();
        gpio_cleanup();
        spi_cleanup();
        return 1;
    }

    display.clear();

    // Init UART
    UART uart;
    if (!uart.open(p.port, p.baud)) {
        std::cerr << "UART open failed\n";
        display.release_cs();
        gpio_cleanup();
        spi_cleanup();
        return 1;
    }

    std::cout << "Waiting for frames...\n\n";

    std::vector<uint8_t> buffer;
    const uint8_t start_bytes[2] = { 0x37, 0x21 };
    int frame_count = 0;
    auto last_report = std::chrono::steady_clock::now();

    while (true)
    {
        auto data = uart.read(256, 50);
        if (data.empty()) {
            continue;
        }

        buffer.insert(buffer.end(), data.begin(), data.end());

        while (buffer.size() >= 5) {
            auto it = std::search(buffer.begin(), buffer.end(),
                                  start_bytes, start_bytes + 2);
            if (it == buffer.end()) {
                buffer.clear();
                break;
            }

            buffer.erase(buffer.begin(), it);
            if (buffer.size() < 5) break;

            uint8_t max_X = buffer[2];
            uint8_t max_Y = buffer[3];
            size_t frame_size = 5 + static_cast<size_t>(max_X) * max_Y;

            if (buffer.size() < frame_size) break;

            if (buffer[frame_size - 1] != FRAME_END) {
                buffer.erase(buffer.begin());
                continue;
            }

            try {
                std::vector<uint8_t> raw(buffer.begin(), buffer.begin() + frame_size);
                Frame f = deserialize(raw);

                display.set_frame(f);
                display.refresh();

                frame_count++;
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration<float>(now - last_report).count();
                if (elapsed >= 1.0f) {
                    std::cout << "\rFrame: " << frame_count << " FPS"
                              << " (" << (int)max_X << "x" << (int)max_Y << ")"
                              << "     " << std::flush;
                    frame_count = 0;
                    last_report = now;
                }

                buffer.erase(buffer.begin(), buffer.begin() + frame_size);
            } catch (const std::exception &e) {
                std::cerr << "\nFrame error: " << e.what() << "\n";
                buffer.erase(buffer.begin());
            }
        }
    }

    // Cleanup (unreachable without Ctrl+C)
    uart.close();
    display.clear();
    display.release_cs();
    gpio_cleanup();
    spi_cleanup();
    return 0;
}
