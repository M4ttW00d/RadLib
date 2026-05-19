// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>

namespace d3s
{

constexpr int SPECTRUM_BINS    = 4096;
constexpr int SPECTRUM_PACKET  = 8205;  // bytes returned by GET_16_BIT_SPECTRUM
constexpr int STATUS_PACKET    = 17;    // bytes returned by GET_STATUS
constexpr int ABOUT_PACKET     = 111;   // bytes returned by GET_ABOUT

struct SpectrumData
{
    uint32_t scan_time_ms;
    uint16_t neutron_count;
    uint16_t channels[SPECTRUM_BINS];
};

/**
 * Thin RAII wrapper around the Kromek D3S serial device.
 * The D3S connects via Bluetooth SPP and appears as /dev/ttyACM*.
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

    // port: empty = auto-detect first /dev/ttyACM* device that responds as a D3S
    bool open(const std::string & port = "");
    void close();
    bool isOpen() const { return fd_ >= 0; }

    // Request one 4096-bin spectrum from the device.
    bool getSpectrum(SpectrumData & data_out);

    // Read current detector temperature (signed °C).
    bool getTemperature(int8_t & celsius_out);

    // Read device serial number (starts with "SGM" on a valid D3S).
    bool getSerialNumber(std::string & serial_out);

private:
    bool    findPort(std::string & port_out);
    bool    configurePort();
    ssize_t readExact(uint8_t * buf, size_t n);

    // Serial command bytes (see D3S Bit Commands.md)
    static const uint8_t CMD_GET_SPECTRUM[7];
    static const uint8_t CMD_GET_STATUS[7];
    static const uint8_t CMD_GET_ABOUT[7];

    int fd_{-1};
};

}  // namespace d3s
