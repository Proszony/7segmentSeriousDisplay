#pragma once

#include <cstdint>
#include <string>
#include <vector>

class UART {
public:
    UART();
    ~UART();

    bool open(const std::string& port, unsigned int baudrate);
    void close();
    bool isOpen() const;

    bool write(const std::vector<uint8_t>& data);
    std::vector<uint8_t> read(size_t count, int timeout_ms = 1000);

private:
    int fd;
    bool opened;
    int original_flags;
};