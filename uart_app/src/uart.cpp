#include "uart.h"
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>

UART::UART() : fd(-1), opened(false), original_flags(0) {}

UART::~UART() {
    close();
}

bool UART::open(const std::string& port, unsigned int baudrate) {
    if (opened) {
        close();
    }

    fd = ::open(port.c_str(), O_RDWR | O_NOCTTY);
    bool is_acm = (port.find("ttyAMC") != std::string::npos);
    if (fd < 0) {
        std::cerr << "UART: Failed to open " << port << ": " << strerror(errno) << std::endl;
        return false;
    }

    original_flags = fcntl(fd, F_GETFL);

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "UART: Failed to get terminal attributes: " << strerror(errno) << std::endl;
        ::close(fd);
        fd = -1;
        return false;
    }

    speed_t speed;
    switch (baudrate) {
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 57600:  speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 921600: speed = B921600; break;
        default:
            std::cerr << "UART: Unsupported baudrate " << baudrate << ", using 115200" << std::endl;
            speed = B115200;
    }
    if(!is_acm){
    	cfsetospeed(&tty, speed);
    	cfsetispeed(&tty, speed);
    }

    cfmakeraw(&tty);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CLOCAL;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "UART: Failed to set terminal attributes: " << strerror(errno) << std::endl;
        ::close(fd);
        fd = -1;
        return false;
    }

    tcflush(fd, TCIOFLUSH);

    opened = true;
    std::cout << "UART: Opened " << port << " at " << baudrate << " baud" << std::endl;
    return true;
}

void UART::close() {
    if (fd >= 0) {
        fcntl(fd, F_SETFL, original_flags);
        ::close(fd);
        fd = -1;
        opened = false;
    }
}

bool UART::isOpen() const {
    return opened;
}

bool UART::write(const std::vector<uint8_t>& data) {
    if (!opened || fd < 0) {
        return false;
    }

    ssize_t written = ::write(fd, data.data(), data.size());
    if (written < 0) {
        std::cerr << "UART: Write error: " << strerror(errno) << std::endl;
        return false;
    }
    
    //tcdrain(fd);
    return true;
}

std::vector<uint8_t> UART::read(size_t count, int timeout_ms) {
    std::vector<uint8_t> buffer;

    if (!opened || fd < 0) {
        return buffer;
    }

    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ret < 0) {
        std::cerr << "UART: Select error: " << strerror(errno) << std::endl;
        return buffer;
    }

    if (ret == 0) {
        return buffer;
    }

    buffer.resize(count);
    ssize_t n = ::read(fd, buffer.data(), count);
    if (n < 0) {
        std::cerr << "UART: Read error: " << strerror(errno) << std::endl;
        buffer.clear();
        return buffer;
    }

    buffer.resize(n > 0 ? static_cast<size_t>(n) : 0);
    return buffer;
}
