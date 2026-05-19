// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include "d3s/d3s_comm.hpp"

#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace d3s
{

// ---------------------------------------------------------------------------
// Serial commands (see D3S Bit Commands.md)
// Format: AA 00 00 BB CC 00 00
//   AA = command length (0x07), BB = target device (0x07 = interface board)
//   CC = command code
// ---------------------------------------------------------------------------
const uint8_t D3SDevice::CMD_GET_SPECTRUM[7] = {0x07,0x00,0x00,0x07,0xC1,0x00,0x00};
const uint8_t D3SDevice::CMD_GET_STATUS[7]   = {0x07,0x00,0x00,0x07,0xC5,0x00,0x00};
const uint8_t D3SDevice::CMD_GET_ABOUT[7]    = {0x07,0x00,0x00,0x07,0xC7,0x00,0x00};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void D3SDevice::close()
{
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool D3SDevice::findPort(std::string & port_out)
{
    namespace fs = std::filesystem;
    for (auto & entry : fs::directory_iterator("/dev")) {
        std::string name = entry.path().filename().string();
        if (name.rfind("ttyACM", 0) == 0) {
            port_out = entry.path().string();
            return true;
        }
    }
    return false;
}

bool D3SDevice::configurePort()
{
    struct termios tty{};
    if (tcgetattr(fd_, &tty) != 0) return false;

    cfsetspeed(&tty, B115200);
    tty.c_cflag  = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tty.c_lflag  = 0;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    tty.c_oflag  = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 50;  // 5 s read timeout (covers the large spectrum response)

    return tcsetattr(fd_, TCSANOW, &tty) == 0;
}

bool D3SDevice::open(const std::string & port)
{
    std::string p = port;
    if (p.empty() && !findPort(p)) return false;

    fd_ = ::open(p.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) return false;

    if (!configurePort()) { close(); return false; }
    tcflush(fd_, TCIOFLUSH);

    // Confirm device is a D3S before accepting it
    std::string serial;
    if (!getSerialNumber(serial) || serial.rfind("SGM", 0) != 0) {
        close();
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

ssize_t D3SDevice::readExact(uint8_t * buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd_, buf + total, n - total);
        if (r <= 0) return static_cast<ssize_t>(total);  // timeout or error
        total += static_cast<size_t>(r);
    }
    return static_cast<ssize_t>(total);
}

// ---------------------------------------------------------------------------
// Device commands
// ---------------------------------------------------------------------------

bool D3SDevice::getSerialNumber(std::string & serial_out)
{
    // GET_ABOUT response layout (111 bytes, little-endian):
    //   H(2) B(1) B(1) B(1) H(2) H(2) char[50] char[50] H(2)
    //   Serial number = first 9 chars of the second char[50] block (offset 59)
    tcflush(fd_, TCIOFLUSH);
    if (write(fd_, CMD_GET_ABOUT, 7) != 7) return false;

    uint8_t buf[ABOUT_PACKET];
    if (readExact(buf, ABOUT_PACKET) != ABOUT_PACKET) return false;

    char sn[10] = {};
    std::memcpy(sn, buf + 59, 9);
    serial_out = sn;
    return true;
}

bool D3SDevice::getTemperature(int8_t & celsius_out)
{
    // GET_STATUS response layout (17 bytes, little-endian):
    //   H(2) B(1) B(1) B(1) B(1) B(1) b(1) B(1) B(1) B(1) B(1) b(1) B(1) B(1) H(2)
    //   Temperature = signed byte at offset 7
    tcflush(fd_, TCIOFLUSH);
    if (write(fd_, CMD_GET_STATUS, 7) != 7) return false;

    uint8_t buf[STATUS_PACKET];
    if (readExact(buf, STATUS_PACKET) != STATUS_PACKET) return false;

    celsius_out = static_cast<int8_t>(buf[7]);
    return true;
}

bool D3SDevice::getSpectrum(SpectrumData & data_out)
{
    // GET_16_BIT_SPECTRUM response layout (8205 bytes, little-endian):
    //   H(2) B(1) B(1) B(1) L(4) H(2) H[4096](8192) H(2)
    //   scan_time_ms   = uint32 at offset 5
    //   neutron_count  = uint16 at offset 9
    //   channels[4096] = uint16[4096] at offset 11
    tcflush(fd_, TCIFLUSH);
    if (write(fd_, CMD_GET_SPECTRUM, 7) != 7) return false;

    uint8_t buf[SPECTRUM_PACKET];
    if (readExact(buf, SPECTRUM_PACKET) != SPECTRUM_PACKET) return false;

    std::memcpy(&data_out.scan_time_ms,   buf + 5,  4);
    std::memcpy(&data_out.neutron_count,  buf + 9,  2);
    std::memcpy(data_out.channels,        buf + 11, SPECTRUM_BINS * sizeof(uint16_t));
    return true;
}

}  // namespace d3s
