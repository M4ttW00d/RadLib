// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <hidapi/hidapi.h>

namespace gr1
{

constexpr uint16_t VENDOR_ID     = 0x04D8;
constexpr uint16_t PRODUCT_ID    = 0x0000;
constexpr int      PACKET_SIZE   = 62;
constexpr int      SPECTRUM_BINS = 4096;

/**
 * Thin RAII wrapper around the Kromek GR1 HID device.
 * All methods return true on success, false on failure.
 * Not thread-safe — call from a single thread.
 */
class GR1Device
{
public:
    GR1Device();
    ~GR1Device();

    GR1Device(const GR1Device &) = delete;
    GR1Device & operator=(const GR1Device &) = delete;

    // serial_number: empty = open any connected GR1
    bool open(const std::string & serial_number = "");
    void close();
    bool isOpen() const { return dev_ != nullptr; }

    // Reads one HID packet (10 ms timeout).
    // Fills channels_out with 0 or 1 channel indices (0–4095).
    // Returns false only on a hard read error.
    bool readEvents(std::vector<uint16_t> & channels_out);

private:
    hid_device * dev_{nullptr};
};

}  // namespace gr1
