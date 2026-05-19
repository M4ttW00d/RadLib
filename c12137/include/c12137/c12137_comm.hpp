// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

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

constexpr int BULK_DATA_SIZE = 1000;  // max events per bulk packet
constexpr int GE_TABLE_SIZE  = 4096;  // ADC channels / G(E) table rows

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
    // LM94021 sensor linearisation
    static double voltToCelsius(uint16_t volt);
    static uint16_t swapBytes(uint16_t v) { return static_cast<uint16_t>((v << 8) | (v >> 8)); }

    libusb_context       * ctx_{nullptr};
    libusb_device_handle * dev_{nullptr};
    bool                   driver_was_active_{false};
};

}  // namespace c12137
