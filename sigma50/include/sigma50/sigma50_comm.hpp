// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <hidapi/hidapi.h>

namespace sigma50
{

constexpr uint16_t VENDOR_ID        = 0x04D8;
constexpr uint16_t PRODUCT_ID       = 0x0023;
constexpr int      PACKET_SIZE      = 63;
constexpr int      SPECTRUM_BINS    = 4096;
constexpr uint8_t  REPORT_SPECTRUM  = 0x04;  // packet type byte for spectral data

/**
 * Thin RAII wrapper around the Kromek Sigma 50 HID device.
 * All methods return true on success, false on failure.
 * Not thread-safe — call from a single thread.
 */
class Sigma50Device
{
public:
    Sigma50Device();
    ~Sigma50Device();

    Sigma50Device(const Sigma50Device &) = delete;
    Sigma50Device & operator=(const Sigma50Device &) = delete;

    // serial_number: empty = open any connected Sigma 50
    bool open(const std::string & serial_number = "");
    void close();
    bool isOpen() const { return dev_ != nullptr; }

    // Reads one HID packet (10 ms timeout).
    // Fills channels_out with 0–31 channel indices (0–4095).
    // Returns false only on a hard read error.
    bool readEvents(std::vector<uint16_t> & channels_out);

private:
    hid_device * dev_{nullptr};
};

}  // namespace sigma50
