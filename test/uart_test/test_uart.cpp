#include "uart.h"
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include <vector>

namespace {

struct PtyPair {
    int master = -1;
    std::string slave;
};

bool make_pty(PtyPair &pty)
{
    pty.master = posix_openpt(O_RDWR | O_NOCTTY);
    if (pty.master < 0)
        return false;

    if (grantpt(pty.master) != 0 || unlockpt(pty.master) != 0) {
        close(pty.master);
        pty.master = -1;
        return false;
    }

    char name[256];
    if (ptsname_r(pty.master, name, sizeof(name)) != 0) {
        close(pty.master);
        pty.master = -1;
        return false;
    }

    pty.slave = name;
    return true;
}

void destroy_pty(PtyPair &pty)
{
    if (pty.master >= 0) {
        close(pty.master);
        pty.master = -1;
    }
}

// Reads up to count bytes from a raw fd, waiting at most timeout_ms.
std::vector<uint8_t> read_with_timeout(int fd, size_t count, int timeout_ms)
{
    std::vector<uint8_t> out;
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(timeout_ms);

    while (out.size() < count) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            break;

        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                              deadline - now).count();

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        struct timeval tv;
        tv.tv_sec = static_cast<time_t>(left / 1000);
        tv.tv_usec = static_cast<suseconds_t>((left % 1000) * 1000);

        const int ready = select(fd + 1, &read_fds, nullptr, nullptr, &tv);
        if (ready <= 0)
            break;

        uint8_t chunk[64];
        const size_t want = std::min(count - out.size(), sizeof(chunk));
        const ssize_t n = ::read(fd, chunk, want);
        if (n <= 0)
            break;

        out.insert(out.end(), chunk, chunk + n);
    }

    return out;
}

// UART::read may return fewer bytes than requested; accumulate until complete.
std::vector<uint8_t> uart_read_exact(UART &uart, size_t count, int timeout_ms)
{
    std::vector<uint8_t> out;
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(timeout_ms);

    while (out.size() < count) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            break;

        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                              deadline - now).count();
        if (left <= 0)
            break;

        auto chunk = uart.read(count - out.size(), static_cast<int>(left));
        if (chunk.empty())
            break;

        out.insert(out.end(), chunk.begin(), chunk.end());
    }

    return out;
}

void test_requires_open()
{
    TEST_SECTION("operation before open fails cleanly");

    UART uart;
    CHECK(!uart.isOpen());

    const std::vector<uint8_t> data{0x01, 0x02, 0x03};
    CHECK(!uart.write(data));
    CHECK(uart.read(4, 20).empty());
}

void test_open_rejects_bad_port()
{
    TEST_SECTION("open rejects a nonexistent device");

    UART uart;
    CHECK(!uart.open("/tmp/7seg-uart-no-such-device", 115200));
    CHECK(!uart.isOpen());
}

void test_open_close_lifecycle()
{
    TEST_SECTION("open/close lifecycle");

    PtyPair pty;
    if (!make_pty(pty)) {
        CHECK(false);
        return;
    }

    UART uart;
    CHECK(uart.open(pty.slave, 115200));
    CHECK(uart.isOpen());

    uart.close();
    CHECK(!uart.isOpen());

    // close() must be idempotent.
    uart.close();
    CHECK(!uart.isOpen());

    // Reopen the same device.
    CHECK(uart.open(pty.slave, 115200));
    CHECK(uart.isOpen());
    uart.close();

    destroy_pty(pty);
}

void test_write_to_pty()
{
    TEST_SECTION("write sends exact bytes to the peer");

    PtyPair pty;
    if (!make_pty(pty)) {
        CHECK(false);
        return;
    }

    UART uart;
    CHECK(uart.open(pty.slave, 115200));

    const std::vector<uint8_t> tx{0x37, 0x21, 0x02, 0x02, 0xAA, 0x55, 0x69};
    CHECK(uart.write(tx));

    const auto rx = read_with_timeout(pty.master, tx.size(), 500);
    CHECK_EQ(rx.size(), tx.size());
    CHECK(rx == tx);

    uart.close();
    destroy_pty(pty);
}

void test_read_from_pty()
{
    TEST_SECTION("read receives exact bytes from the peer");

    PtyPair pty;
    if (!make_pty(pty)) {
        CHECK(false);
        return;
    }

    UART uart;
    CHECK(uart.open(pty.slave, 115200));

    const std::vector<uint8_t> tx{0x37, 0x21, 0x01, 0x01, 0x7E, 0x69};
    CHECK_EQ(::write(pty.master, tx.data(), tx.size()), static_cast<ssize_t>(tx.size()));

    const auto rx = uart_read_exact(uart, tx.size(), 500);
    CHECK_EQ(rx.size(), tx.size());
    CHECK(rx == tx);

    uart.close();
    destroy_pty(pty);
}

void test_read_timeout()
{
    TEST_SECTION("read times out on an idle port");

    PtyPair pty;
    if (!make_pty(pty)) {
        CHECK(false);
        return;
    }

    UART uart;
    CHECK(uart.open(pty.slave, 115200));

    const auto begin = std::chrono::steady_clock::now();
    const auto rx = uart.read(8, 120);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - begin).count();

    CHECK(rx.empty());
    CHECK(elapsed >= 80);
    CHECK(elapsed < 2000);

    uart.close();
    destroy_pty(pty);
}

void test_bidirectional()
{
    TEST_SECTION("bidirectional traffic does not interfere");

    PtyPair pty;
    if (!make_pty(pty)) {
        CHECK(false);
        return;
    }

    UART uart;
    CHECK(uart.open(pty.slave, 115200));

    const std::vector<uint8_t> out{0xDE, 0xAD, 0xBE, 0xEF};
    const std::vector<uint8_t> in{0xCA, 0xFE, 0xBA, 0xBE};

    CHECK(uart.write(out));
    CHECK_EQ(::write(pty.master, in.data(), in.size()), static_cast<ssize_t>(in.size()));

    const auto got_out = read_with_timeout(pty.master, out.size(), 500);
    const auto got_in = uart_read_exact(uart, in.size(), 500);

    CHECK(got_out == out);
    CHECK(got_in == in);

    uart.close();
    destroy_pty(pty);
}

}

int main()
{
    test_requires_open();
    test_open_rejects_bad_port();
    test_open_close_lifecycle();
    test_write_to_pty();
    test_read_from_pty();
    test_read_timeout();
    test_bidirectional();
    return test::summary();
}
