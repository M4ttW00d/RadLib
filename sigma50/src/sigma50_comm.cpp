// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include "sigma50/sigma50_comm.hpp"

namespace sigma50
{

Sigma50Device::Sigma50Device()  { hid_init(); }
Sigma50Device::~Sigma50Device() { close(); hid_exit(); }

bool Sigma50Device::open(const std::string & serial_number)
{
    if (serial_number.empty()) {
        dev_ = hid_open(VENDOR_ID, PRODUCT_ID, nullptr);
    } else {
        std::wstring ws(serial_number.begin(), serial_number.end());
        dev_ = hid_open(VENDOR_ID, PRODUCT_ID, ws.c_str());
    }
    return dev_ != nullptr;
}

void Sigma50Device::close()
{
    if (dev_) { hid_close(dev_); dev_ = nullptr; }
}

bool Sigma50Device::readEvents(std::vector<uint16_t> & channels_out)
{
    channels_out.clear();
    uint8_t buf[PACKET_SIZE];

    // Block up to 10 ms; returns 0 on timeout, -1 on error
    int r = hid_read_timeout(dev_, buf, PACKET_SIZE, 10);
    if (r < 0) return false;
    if (r < 3 || buf[0] != REPORT_SPECTRUM) return true;

    // Each spectral packet contains up to 31 event pairs (bytes 1–62).
    // Each pair: channel = (byte1 << 4) | (byte2 >> 4)
    //            valid   = LSB of byte2; zero means no more events in this packet.
    for (int i = 1; i <= 61; i += 2) {
        uint8_t byte1 = buf[i];
        uint8_t byte2 = buf[i + 1];
        if (!(byte2 & 0x01)) break;
        channels_out.push_back(static_cast<uint16_t>((byte1 << 4) | (byte2 >> 4)));
    }
    return true;
}

}  // namespace sigma50
