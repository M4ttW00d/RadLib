// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include "c12137/c12137_comm.hpp"

#include <cstring>
#include <unistd.h>

namespace c12137
{

// ---------------------------------------------------------------------------
// USB vendor request types
// ---------------------------------------------------------------------------
namespace
{
constexpr uint8_t REQ_OUT = LIBUSB_REQUEST_TYPE_VENDOR |
                            LIBUSB_RECIPIENT_OTHER    |
                            LIBUSB_ENDPOINT_OUT;   // 0x43

constexpr uint8_t REQ_IN  = LIBUSB_REQUEST_TYPE_VENDOR |
                            LIBUSB_RECIPIENT_OTHER    |
                            LIBUSB_ENDPOINT_IN;    // 0xC3

// Vendor request codes
constexpr uint8_t REQ_RESET          = 0x01;
constexpr uint8_t REQ_READ_EEPROM    = 0x04;
constexpr uint8_t REQ_RADIATION_LIMIT= 0x07;
constexpr uint8_t REQ_ENERGY_THRESH  = 0x0C;
constexpr uint8_t REQ_I2C_RXD        = 0x22;
constexpr uint8_t REQ_CLEAR_BULK     = 0xF0;

constexpr uint8_t I2C_TEMP_ANALOG    = 3;

// Bulk endpoint: endpoint 2, IN direction (0x80 | 0x02)
constexpr uint8_t BULK_EP            = 0x82;
constexpr int     BULK_HEADER_WORDS  = 8;
constexpr int     BULK_TOTAL_WORDS   = BULK_HEADER_WORDS + 1048;  // = 1056
constexpr int     BULK_TIMEOUT_MS    = 200;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

double C12137Device::voltToCelsius(uint16_t volt)
{
    // LM94021 GS0=0 GS1=0: 0 °C → 1034 mV, 50 °C → 760 mV
    // ADC full-scale = 65535 counts = 1250 mV
    return -0.00348 * static_cast<double>(volt) + 188.686;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

C12137Device::C12137Device() = default;

C12137Device::~C12137Device()
{
    close();
}

bool C12137Device::open()
{
    if (libusb_init(&ctx_) < 0) return false;

    dev_ = libusb_open_device_with_vid_pid(ctx_, VENDOR_ID, PRODUCT_ID);
    if (!dev_) {
        libusb_exit(ctx_);
        ctx_ = nullptr;
        return false;
    }

    // Detach any kernel HID driver so we can claim the interface
    driver_was_active_ = (libusb_kernel_driver_active(dev_, 0) == 1);
    if (driver_was_active_) {
        if (libusb_detach_kernel_driver(dev_, 0) < 0) {
            libusb_close(dev_);
            libusb_exit(ctx_);
            dev_ = nullptr;
            ctx_ = nullptr;
            return false;
        }
    }

    if (libusb_set_configuration(dev_, 1) < 0 ||
        libusb_claim_interface(dev_, 0) < 0)
    {
        if (driver_was_active_) libusb_attach_kernel_driver(dev_, 0);
        libusb_close(dev_);
        libusb_exit(ctx_);
        dev_ = nullptr;
        ctx_ = nullptr;
        return false;
    }

    return true;
}

void C12137Device::close()
{
    if (dev_) {
        libusb_release_interface(dev_, 0);
        if (driver_was_active_) libusb_attach_kernel_driver(dev_, 0);
        libusb_close(dev_);
        dev_ = nullptr;
    }
    if (ctx_) {
        libusb_exit(ctx_);
        ctx_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Device commands
// ---------------------------------------------------------------------------

bool C12137Device::reset(uint16_t level)
{
    int r = libusb_control_transfer(dev_, REQ_OUT, REQ_RESET,
                                    level, 0, nullptr, 0, TIMEOUT_MS);
    usleep(100'000);  // 100 ms — device requires settling time after reset
    return r >= 0;
}

bool C12137Device::getEnergyThreshold(uint16_t & index_out)
{
    uint16_t buf = 0;
    int r = libusb_control_transfer(dev_, REQ_IN, REQ_ENERGY_THRESH,
                                    0, 0,
                                    reinterpret_cast<uint8_t *>(&buf), 2,
                                    TIMEOUT_MS);
    if (r < 2) return false;
    index_out = swapBytes(buf);
    return true;
}

bool C12137Device::setEnergyThreshold(uint16_t index)
{
    int r = libusb_control_transfer(dev_, REQ_OUT, REQ_ENERGY_THRESH,
                                    index, 0, nullptr, 0, TIMEOUT_MS);
    return r >= 0;
}

bool C12137Device::getRadiationLimit(uint16_t area, uint16_t & kev_out)
{
    uint16_t buf = 0;
    int r = libusb_control_transfer(dev_, REQ_IN, REQ_RADIATION_LIMIT,
                                    0, area,
                                    reinterpret_cast<uint8_t *>(&buf), 2,
                                    TIMEOUT_MS);
    if (r < 2) return false;
    kev_out = swapBytes(buf);
    return true;
}

bool C12137Device::setRadiationLimit(uint16_t area, uint16_t kev)
{
    int r = libusb_control_transfer(dev_, REQ_OUT, REQ_RADIATION_LIMIT,
                                    kev, area, nullptr, 0, TIMEOUT_MS);
    return r >= 0;
}

bool C12137Device::getDataAndTemperature(uint16_t & index, uint16_t & size,
                                         uint16_t * data,
                                         double & temperature_celsius)
{
    /**
     * Bulk packet layout (uint16, big-endian header / little-endian data):
     *   [0] 0x5A5A  [1] 0x5A5A   — sync words
     *   [2] count                 — number of events (0–1000)
     *   [3] 0x0000
     *   [4] index                 — increments every 100 ms
     *   [5] temperature ADC
     *   [6] 0x0000  [7] debug
     *   [8..1007]   event words   — top 12 bits = ADC channel
     *   [1008..1055] padding
     */
    uint16_t buf[BULK_TOTAL_WORDS];
    int transferred = 0;

    int r = libusb_bulk_transfer(dev_, BULK_EP,
                                  reinterpret_cast<uint8_t *>(buf),
                                  BULK_TOTAL_WORDS * sizeof(uint16_t),
                                  &transferred,
                                  BULK_TIMEOUT_MS);

    if (r != LIBUSB_SUCCESS ||
        transferred != BULK_TOTAL_WORDS * static_cast<int>(sizeof(uint16_t)))
    {
        return false;
    }

    // Byte-swap the header words (device sends them big-endian)
    for (int i = 0; i < BULK_HEADER_WORDS; i++) {
        buf[i] = swapBytes(buf[i]);
    }

    if (buf[0] != 0x5A5A || buf[1] != 0x5A5A) return false;

    index = buf[4];
    size  = (buf[2] <= BULK_DATA_SIZE) ? buf[2] : BULK_DATA_SIZE;
    std::memcpy(data, &buf[BULK_HEADER_WORDS], size * sizeof(uint16_t));
    temperature_celsius = voltToCelsius(buf[5]);

    return true;
}

bool C12137Device::clearBulkBuffer()
{
    int r = libusb_control_transfer(dev_, REQ_OUT, REQ_CLEAR_BULK,
                                    0, 0, nullptr, 0, TIMEOUT_MS);
    return r >= 0;
}

bool C12137Device::readEeprom(uint16_t address, uint16_t & data_out)
{
    uint16_t buf = 0;
    int r = libusb_control_transfer(dev_, REQ_IN, REQ_READ_EEPROM,
                                    address, 0x02,
                                    reinterpret_cast<uint8_t *>(&buf), 2,
                                    TIMEOUT_MS);
    if (r < 2) return false;
    data_out = swapBytes(buf);
    return true;
}

bool C12137Device::getHvpsTemperature(double & celsius_out, uint16_t & raw_digit_out)
{
    // Two-phase I2C request: send request, wait, then read result
    int r = libusb_control_transfer(dev_, REQ_OUT, REQ_I2C_RXD,
                                    I2C_TEMP_ANALOG, 0,
                                    nullptr, 0, TIMEOUT_MS);
    if (r < 0) return false;

    usleep(100'000);  // 100 ms for I2C round-trip

    uint16_t buf = 0;
    r = libusb_control_transfer(dev_, REQ_IN, REQ_I2C_RXD,
                                 0, 0,
                                 reinterpret_cast<uint8_t *>(&buf), 2,
                                 TIMEOUT_MS);
    if (r < 2) return false;

    uint16_t value = swapBytes(buf);
    raw_digit_out = value;

    // Same LM94021 conversion used in the HVPS path
    double mV = static_cast<double>(value) * 1250.0 / 65535.0;
    celsius_out = ((mV - 1034.0) * 50.0) / (760.0 - 1034.0);

    return true;
}

}  // namespace c12137
