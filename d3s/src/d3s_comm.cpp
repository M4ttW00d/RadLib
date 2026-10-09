// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include "d3s/d3s_comm.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <system_error>
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
// Frame decoders
// ---------------------------------------------------------------------------

bool decodeSpectrumFrame(const uint8_t * frame, size_t length, SpectrumData & data_out)
{
    if (length < static_cast<size_t>(SPECTRUM_PACKET)) return false;

    std::memcpy(&data_out.scan_time_ms,  frame + 5,  sizeof(data_out.scan_time_ms));
    std::memcpy(&data_out.neutron_count, frame + 9,  sizeof(data_out.neutron_count));
    std::memcpy(data_out.channels,       frame + 11, sizeof(data_out.channels));
    return true;
}

bool decodeStatusFrame(const uint8_t * frame, size_t length, int8_t & celsius_out)
{
    if (length < static_cast<size_t>(STATUS_PACKET)) return false;

    celsius_out = static_cast<int8_t>(frame[7]);
    return true;
}

bool decodeAboutFrame(const uint8_t * frame, size_t length, std::string & serial_out)
{
    if (length < static_cast<size_t>(ABOUT_PACKET)) return false;

    const char * start = reinterpret_cast<const char *>(frame + 59);
    serial_out.assign(start, strnlen(start, 9));
    return true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void D3SDevice::close()
{
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

std::vector<std::string> D3SDevice::candidatePorts()
{
    namespace fs = std::filesystem;
    std::vector<std::string> acm, rfcomm;
    std::error_code ec;
    for (const auto & entry : fs::directory_iterator("/dev", ec)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("ttyACM", 0) == 0) {
            acm.push_back(entry.path().string());
        } else if (name.rfind("rfcomm", 0) == 0) {
            rfcomm.push_back(entry.path().string());
        }
    }
    std::sort(acm.begin(), acm.end());
    std::sort(rfcomm.begin(), rfcomm.end());
    acm.insert(acm.end(), rfcomm.begin(), rfcomm.end());
    return acm;
}

bool D3SDevice::configurePort()
{
    struct termios tty{};
    if (tcgetattr(fd_, &tty) != 0) return false;

    cfmakeraw(&tty);  // 8-bit binary: no echo, line editing, flow control or CR/NL mapping
    cfsetspeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~static_cast<tcflag_t>(PARENB | CSTOPB | CRTSCTS);
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 50;  // 5 s read timeout (covers the large spectrum response)

    return tcsetattr(fd_, TCSANOW, &tty) == 0;
}

bool D3SDevice::openPort(const std::string & port)
{
    close();

    fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) return false;

    if (!configurePort()) { close(); return false; }
    tcflush(fd_, TCIOFLUSH);

    // Confirm the device is a D3S before accepting it
    std::string serial;
    if (!getSerialNumber(serial) || serial.rfind("SGM", 0) != 0) {
        close();
        return false;
    }

    return true;
}

bool D3SDevice::open(const std::string & port)
{
    if (!port.empty()) return openPort(port);

    for (const std::string & candidate : candidatePorts()) {
        if (openPort(candidate)) return true;
    }
    return false;
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
    if (fd_ < 0) return false;

    tcflush(fd_, TCIOFLUSH);
    if (write(fd_, CMD_GET_ABOUT, 7) != 7) return false;

    uint8_t buf[ABOUT_PACKET];
    if (readExact(buf, ABOUT_PACKET) != ABOUT_PACKET) return false;

    return decodeAboutFrame(buf, ABOUT_PACKET, serial_out);
}

bool D3SDevice::getTemperature(int8_t & celsius_out)
{
    if (fd_ < 0) return false;

    tcflush(fd_, TCIOFLUSH);
    if (write(fd_, CMD_GET_STATUS, 7) != 7) return false;

    uint8_t buf[STATUS_PACKET];
    if (readExact(buf, STATUS_PACKET) != STATUS_PACKET) return false;

    return decodeStatusFrame(buf, STATUS_PACKET, celsius_out);
}

bool D3SDevice::getSpectrum(SpectrumData & data_out)
{
    if (fd_ < 0) return false;

    tcflush(fd_, TCIFLUSH);
    if (write(fd_, CMD_GET_SPECTRUM, 7) != 7) return false;

    uint8_t buf[SPECTRUM_PACKET];
    if (readExact(buf, SPECTRUM_PACKET) != SPECTRUM_PACKET) return false;

    return decodeSpectrumFrame(buf, SPECTRUM_PACKET, data_out);
}

}  // namespace d3s
