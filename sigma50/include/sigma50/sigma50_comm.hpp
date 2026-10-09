// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <hidapi/hidapi.h>

namespace sigma50
{

constexpr uint16_t VENDOR_ID        = 0x04D8;
constexpr uint16_t PRODUCT_ID       = 0x0023;
constexpr int      PACKET_SIZE      = 63;    // report ID byte + 31 two-byte event slots
constexpr int      SPECTRUM_BINS    = 4096;
constexpr uint8_t  REPORT_SPECTRUM  = 0x04;  // report ID of packets carrying event data

/**
 * Decodes one raw HID report into channel indices.
 *
 * Report layout (shared by the Kromek Sigma 50 and GR1):
 *   [0]      report ID; 0x04 for event data, other values are configuration replies
 *   [1..62]  31 two-byte slots. For each slot:
 *              channel = (byte1 << 4) | (byte2 >> 4)   (12 bits, 0-4095)
 *              bit 0 of byte2 set   -> the slot holds an event
 *              bit 0 of byte2 clear -> no more events in this report
 *
 * Reports that are not event data, or shorter than PACKET_SIZE, yield no events.
 * Returns the number of channel indices appended to channels_out.
 */
size_t decodeReport(const uint8_t * report, size_t length,
                    std::vector<uint16_t> & channels_out);

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

    // serial_number: empty = open the first Sigma 50 found; otherwise the unit
    // whose USB serial string matches exactly. A unit that reports no serial
    // string can only be opened with an empty serial_number.
    bool open(const std::string & serial_number = "");
    void close();
    bool isOpen() const { return dev_ != nullptr; }

    // Reads one HID report (10 ms timeout) and decodes it with decodeReport().
    // Fills channels_out with 0–31 channel indices (0–4095).
    // Returns false only on a hard read error.
    bool readEvents(std::vector<uint16_t> & channels_out);

private:
    hid_device * dev_{nullptr};
};

}  // namespace sigma50
