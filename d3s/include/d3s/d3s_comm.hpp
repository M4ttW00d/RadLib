// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <sys/types.h>

namespace d3s
{

constexpr int SPECTRUM_BINS    = 4096;
constexpr int SPECTRUM_PACKET  = 8205;  // bytes returned by GET_16_BIT_SPECTRUM
constexpr int STATUS_PACKET    = 17;    // bytes returned by GET_STATUS
constexpr int ABOUT_PACKET     = 111;   // bytes returned by GET_ABOUT

struct SpectrumData
{
    uint32_t scan_time_ms;    // real time this frame covers (since the previous request)
    uint16_t neutron_count;   // neutron counts in that time
    uint16_t channels[SPECTRUM_BINS];
};

// Frame decoders. Each returns false if `length` is shorter than the frame.

// GET_16_BIT_SPECTRUM reply (8205 bytes, little-endian):
//   H(2) B(1) B(1) B(1) L(4) H(2) H[4096](8192) H(2)
//   scan_time_ms   = uint32 at offset 5
//   neutron_count  = uint16 at offset 9
//   channels[4096] = uint16[4096] at offset 11
bool decodeSpectrumFrame(const uint8_t * frame, size_t length, SpectrumData & data_out);

// GET_STATUS reply (17 bytes, little-endian):
//   H(2) B(1) B(1) B(1) B(1) B(1) b(1) B(1) B(1) B(1) B(1) b(1) B(1) B(1) H(2)
//   temperature = signed byte at offset 7
bool decodeStatusFrame(const uint8_t * frame, size_t length, int8_t & celsius_out);

// GET_ABOUT reply (111 bytes, little-endian):
//   H(2) B(1) B(1) B(1) H(2) H(2) char[50] char[50] H(2)
//   serial number = first 9 chars of the second char[50] block (offset 59)
bool decodeAboutFrame(const uint8_t * frame, size_t length, std::string & serial_out);

/**
 * Thin RAII wrapper around the Kromek D3S serial link.
 *
 * The D3S presents a 115200 baud serial port either over USB (CDC-ACM,
 * /dev/ttyACM*) or over Bluetooth SPP (/dev/rfcomm* once paired and bound
 * with rfcomm). The protocol is the same on both.
 *
 * Every GET_16_BIT_SPECTRUM reply holds the counts accumulated since the
 * previous request, so the first reply after a pause covers the whole pause.
 * D3SDriverT discards that first frame; code using this class directly should
 * do the same.
 *
 * All methods return true on success, false on failure.
 * Not thread-safe — call from a single thread.
 */
class D3SDevice
{
public:
    D3SDevice()  = default;
    ~D3SDevice() { close(); }

    D3SDevice(const D3SDevice &) = delete;
    D3SDevice & operator=(const D3SDevice &) = delete;

    // port: a device path such as /dev/ttyACM0 or /dev/rfcomm0.
    // Empty = probe every /dev/ttyACM* then every /dev/rfcomm* and keep the
    // first one that answers GET_ABOUT with a serial number starting "SGM".
    bool open(const std::string & port = "");
    void close();
    bool isOpen() const { return fd_ >= 0; }

    // Request one 4096-bin spectrum from the device.
    bool getSpectrum(SpectrumData & data_out);

    // Read current detector temperature (signed °C).
    bool getTemperature(int8_t & celsius_out);

    // Read device serial number (starts with "SGM" on a valid D3S).
    bool getSerialNumber(std::string & serial_out);

    // Candidate ports in probe order: /dev/ttyACM* sorted, then /dev/rfcomm* sorted.
    static std::vector<std::string> candidatePorts();

private:
    bool    openPort(const std::string & port);
    bool    configurePort();
    ssize_t readExact(uint8_t * buf, size_t n);

    // Serial command bytes (see D3S Bit Commands.md)
    static const uint8_t CMD_GET_SPECTRUM[7];
    static const uint8_t CMD_GET_STATUS[7];
    static const uint8_t CMD_GET_ABOUT[7];

    int fd_{-1};
};

}  // namespace d3s
