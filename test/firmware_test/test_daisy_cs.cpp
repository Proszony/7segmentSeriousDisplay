#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>

#define REG_NOOP       0x00
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

#define CS_GPIO        25
#define MAX_CHIPS      6

static const char *SPI_DEV = "/dev/spidev0.0";
static const uint32_t SPI_SPEED = 1000000;
static int spi_fd = -1;
static struct gpiod_chip *chip = NULL;
static struct gpiod_line *cs_line = NULL;

static int num_chips = 2;

static void print_help(void)
{
    std::cout << "Usage: test_daisy_cs [options]\n";
    std::cout << "  --chips <n>  MAX7219 in daisy chain (default: 2, max: " << MAX_CHIPS << ")\n";
    std::cout << "  --help       This message\n";
}

static bool parse_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--chips" && i + 1 < argc) {
            num_chips = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            print_help();
            std::exit(0);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }
    if (num_chips < 1 || num_chips > MAX_CHIPS) {
        std::cerr << "chips must be 1.." << MAX_CHIPS << "\n";
        return false;
    }
    return true;
}

static bool gpio_init(void)
{
    chip = gpiod_chip_open_by_number(0);
    if (!chip) { std::cerr << "gpiod_chip_open: " << strerror(errno) << "\n"; return false; }
    cs_line = gpiod_chip_get_line(chip, CS_GPIO);
    if (!cs_line) { std::cerr << "gpiod_get_line(" << CS_GPIO << "): " << strerror(errno) << "\n"; return false; }
    if (gpiod_line_request_output(cs_line, "max7219", 1) < 0) {
        std::cerr << "gpiod_request_output: " << strerror(errno) << "\n"; return false;
    }
    return true;
}

static void gpio_cleanup(void)
{
    if (cs_line) { gpiod_line_set_value(cs_line, 1); gpiod_line_release(cs_line); cs_line = NULL; }
    if (chip) { gpiod_chip_close(chip); chip = NULL; }
}

static void cs_low(void)  { if (cs_line) gpiod_line_set_value(cs_line, 0); }
static void cs_high(void) { if (cs_line) gpiod_line_set_value(cs_line, 1); }

static bool spi_open(void)
{
    spi_fd = open(SPI_DEV, O_RDWR);
    if (spi_fd < 0) {
        std::cerr << "Cannot open " << SPI_DEV << ": " << strerror(errno) << "\n";
        std::cerr << "  Enable SPI: raspi-config -> Interface Options -> SPI\n";
        return false;
    }
    uint8_t mode = SPI_MODE_0 | SPI_NO_CS;
    uint8_t bits = 8;
    uint32_t speed = SPI_SPEED;
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        std::cerr << "SPI config failed: " << strerror(errno) << "\n";
        close(spi_fd); spi_fd = -1; return false;
    }
    return true;
}

static void spi_close(void) { if (spi_fd >= 0) { close(spi_fd); spi_fd = -1; } }

// Send N words (N*2 bytes) in one ioctl with CS pulse
// Buffer: word 0 → farthest chip, word N-1 → closest chip
static void batch_send(const uint8_t *buf, int words)
{
    struct spi_ioc_transfer tr = {};
    tr.tx_buf = (unsigned long)buf;
    tr.len = words * 2;
    tr.speed_hz = SPI_SPEED;
    tr.bits_per_word = 8;
    cs_low();
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0)
        std::cerr << "SPI error: " << strerror(errno) << "\n";
    cs_high();
    usleep(10);
}

static void daisy_init(void)
{
    struct { uint8_t r, v; } cmds[] = {
        {REG_SHUTDOWN, 0x01}, {REG_DECODE, 0x00},
        {REG_SCANLIMIT, 0x07}, {REG_INTENSITY, 0x08},
        {REG_DISPTEST, 0x00},
    };
    for (auto &c : cmds) {
        uint8_t buf[MAX_CHIPS * 2];
        for (int ci = 0; ci < num_chips; ci++)
            buf[ci * 2 + 0] = c.r, buf[ci * 2 + 1] = c.v;
        batch_send(buf, num_chips);
    }
}

static void daisy_set_cell(int y, int pos, uint8_t val)
{
    if (y < 0 || y >= num_chips || pos < 0 || pos > 7) return;
    uint8_t reg = REG_DIGIT0 + (7 - pos);
    uint8_t buf[MAX_CHIPS * 2];
    for (int ci = 0; ci < num_chips; ci++) {
        int chip = num_chips - 1 - ci;
        buf[ci * 2 + 0] = (chip == y) ? reg : REG_NOOP;
        buf[ci * 2 + 1] = (chip == y) ? val : 0x00;
    }
    batch_send(buf, num_chips);
}

static void daisy_set_all(uint8_t val)
{
    for (int d = 0; d < 8; d++) {
        uint8_t buf[MAX_CHIPS * 2];
        for (int ci = 0; ci < num_chips; ci++)
            buf[ci * 2 + 0] = REG_DIGIT0 + d, buf[ci * 2 + 1] = val;
        batch_send(buf, num_chips);
    }
}

static void daisy_clear(void)
{
    for (int d = 0; d < 8; d++) {
        uint8_t buf[MAX_CHIPS * 2];
        for (int ci = 0; ci < num_chips; ci++)
            buf[ci * 2 + 0] = REG_DIGIT0 + d, buf[ci * 2 + 1] = 0x00;
        batch_send(buf, num_chips);
    }
}

static const char *hex_name[16] = {
    "0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", "A", "b", "C", "d", "E", "F",
};

static const uint8_t hex_val[16] = {
    0x7E, 0x30, 0x6D, 0x79,
    0x33, 0x5B, 0x5F, 0x70,
    0x7F, 0x7B, 0x77, 0x1F,
    0x4E, 0x3D, 0x4F, 0x47,
};

static const char *seg_name[7] = {"G", "F", "E", "D", "C", "B", "A"};

static void test_walk(void)
{
    std::cout << "\n[1/4] Each chip lights all 8 digits\n";
    for (int y = 0; y < num_chips; y++) {
        daisy_clear();
        for (int pos = 0; pos < 8; pos++)
            daisy_set_cell(y, pos, 0x7F);
        std::cout << "  Chip " << y << " all 8 digits lit\n";
        usleep(1000000);
    }
    daisy_clear();
}

static void test_hex(void)
{
    std::cout << "\n[2/4] Hex 0-F on each chip & digit\n";
    for (int y = 0; y < num_chips; y++) {
        std::cout << "  Chip " << y << ":\n";
        for (int pos = 0; pos < 8; pos++) {
            std::cout << "    Digit " << pos << ": ";
            for (int h = 0; h < 16; h++) {
                daisy_clear();
                daisy_set_cell(y, pos, hex_val[h]);
                std::cout << hex_name[h] << " " << std::flush;
                usleep(100000);
            }
            std::cout << "\n";
        }
    }
    daisy_clear();
}

static void test_all_same(void)
{
    std::cout << "\n[3/4] All " << (num_chips * 8) << " digits showing same hex value\n";
    for (int h = 0; h < 16; h++) {
        daisy_set_all(hex_val[h]);
        std::cout << "  Value: " << hex_name[h] << "\n";
        usleep(600000);
    }
    daisy_clear();
}

static void test_segments(void)
{
    std::cout << "\n[4/4] Single segment walk on each chip\n";
    for (int y = 0; y < num_chips; y++) {
        std::cout << "  Chip " << y << ":\n";
        for (int pos = 0; pos < 8; pos++) {
            std::cout << "    Digit " << pos << ": ";
            for (int s = 0; s < 7; s++) {
                daisy_clear();
                daisy_set_cell(y, pos, 1u << s);
                std::cout << seg_name[s] << " " << std::flush;
                usleep(200000);
            }
            std::cout << "\n";
        }
    }
    daisy_clear();
}

int main(int argc, char **argv)
{
    if (!parse_args(argc, argv)) return 1;

    std::cout << "MAX7219 Daisy-Chain Test (" << num_chips << " chips)\n";
    std::cout << "==========================================\n";
    std::cout << "SPI: " << SPI_DEV << " @ " << (SPI_SPEED / 1000000) << " MHz\n";
    std::cout << "CS:  GPIO " << CS_GPIO << " (pin 22), shared for all chips\n";
    std::cout << "Chip 0 = closest to RPi, Chip " << (num_chips - 1) << " = farthest\n";
    std::cout << "Press Ctrl+C to abort.\n\n";

    if (!gpio_init()) return 1;
    if (!spi_open()) { gpio_cleanup(); return 1; }

    std::cout << "Initializing...\n";
    daisy_init();
    daisy_clear();
    usleep(300000);

    test_walk();
    test_hex();
    test_all_same();
    test_segments();

    std::cout << "\nDone. Shutting down.\n";
    daisy_clear();
    gpio_cleanup();
    spi_close();
    return 0;
}
