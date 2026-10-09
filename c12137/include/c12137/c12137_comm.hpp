// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <libusb-1.0/libusb.h>

namespace c12137
{

constexpr uint16_t VENDOR_ID  = 0x0661;
constexpr uint16_t PRODUCT_ID = 0x2917;
constexpr uint32_t TIMEOUT_MS = 5000;

// EEPROM addresses
constexpr uint16_t EEPROM_COMP_LEVEL   = 0x0A;  // comparator threshold default (keV)
constexpr uint16_t EEPROM_ENERGY_LOWER = 0x0C;  // lower energy range limit default (keV)
constexpr uint16_t EEPROM_ENERGY_UPPER = 0x0E;  // upper energy range limit default (keV)
constexpr uint16_t EEPROM_CONVERT_USV  = 0x10;  // calibration coefficient (default 1000 = 1x)

constexpr int BULK_DATA_SIZE    = 1000;  // max events per bulk packet
constexpr int GE_TABLE_SIZE     = 4096;  // ADC channels / G(E) table rows
constexpr int BULK_HEADER_WORDS = 8;                         // uint16 words of header per bulk packet
constexpr int BULK_TOTAL_WORDS  = BULK_HEADER_WORDS + 1048;  // = 1056 words = 2112 bytes

inline uint16_t swap16(uint16_t v) { return static_cast<uint16_t>((v << 8) | (v >> 8)); }

/**
 * Converts the temperature ADC digit carried in each bulk packet to °C
 * (LM94021 at GS0=0 GS1=0; 16-bit ADC with 1250 mV full scale).
 */
double adcToCelsius(uint16_t adc_digit);

/**
 * Decodes one raw bulk packet.
 *
 * `words` is the packet exactly as read from the endpoint: BULK_TOTAL_WORDS
 * uint16 values in host order. The eight header words are sent big-endian and
 * are byte-swapped here; the event words are little-endian and used as they are.
 *   [0] 0x5A5A  [1] 0x5A5A   sync words
 *   [2] count                number of events (0–1000)
 *   [3] 0x0000
 *   [4] index                increments every 100 ms
 *   [5] temperature ADC
 *   [6] 0x0000  [7] debug
 *   [8..1007]   event words  top 12 bits = ADC channel
 *   [1008..1055] padding
 *
 * `data` must have room for BULK_DATA_SIZE words; `size` is clamped to that.
 * Returns false if the packet is short or the sync words are missing.
 */
bool decodeBulkPacket(const uint16_t * words, size_t word_count,
                      uint16_t & index, uint16_t & size,
                      uint16_t * data, double & temperature_celsius);

/**
 * Thin RAII wrapper around the C12137 USB device.
 * All methods return true on success, false on failure.
 * Not thread-safe — call from a single thread.
 */
class C12137Device
{
public:
    C12137Device();
    ~C12137Device();

    C12137Device(const C12137Device &) = delete;
    C12137Device & operator=(const C12137Device &) = delete;

    bool open();
    void close();
    bool isOpen() const { return dev_ != nullptr; }

    // Module control
    bool reset(uint16_t level = 0);

    // Comparator threshold (units: ADC index, convert via G(E) table)
    bool getEnergyThreshold(uint16_t & index_out);
    bool setEnergyThreshold(uint16_t index);

    // Energy window for dose-rate integration (area: 0=lower, 1=upper; units: keV)
    bool getRadiationLimit(uint16_t area, uint16_t & kev_out);
    bool setRadiationLimit(uint16_t area, uint16_t kev);

    // Primary data acquisition — call every ~20 ms
    // index: packet counter (increments every 100 ms on the device)
    // size:  number of events in this packet (0–1000)
    // data:  caller-allocated buffer of at least BULK_DATA_SIZE uint16s
    bool getDataAndTemperature(uint16_t & index, uint16_t & size,
                               uint16_t * data, double & temperature_celsius);

    bool clearBulkBuffer();

    // EEPROM read (address: one of the EEPROM_* constants above)
    bool readEeprom(uint16_t address, uint16_t & data_out);

    // HVPS board temperature (separate sensor from the bulk-packet temperature)
    bool getHvpsTemperature(double & celsius_out, uint16_t & raw_digit_out);

private:
    libusb_context       * ctx_{nullptr};
    libusb_device_handle * dev_{nullptr};
    bool                   driver_was_active_{false};
};

}  // namespace c12137
