// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include "gr1/gr1_comm.hpp"

namespace gr1
{

GR1Device::GR1Device()  { hid_init(); }
GR1Device::~GR1Device() { close(); hid_exit(); }

bool GR1Device::open(const std::string & serial_number)
{
    if (serial_number.empty()) {
        dev_ = hid_open(VENDOR_ID, PRODUCT_ID, nullptr);
    } else {
        std::wstring ws(serial_number.begin(), serial_number.end());
        dev_ = hid_open(VENDOR_ID, PRODUCT_ID, ws.c_str());
    }
    return dev_ != nullptr;
}

void GR1Device::close()
{
    if (dev_) { hid_close(dev_); dev_ = nullptr; }
}

bool GR1Device::readEvents(std::vector<uint16_t> & channels_out)
{
    channels_out.clear();
    uint8_t buf[PACKET_SIZE];

    // Block up to 10 ms; returns 0 on timeout, -1 on error
    int r = hid_read_timeout(dev_, buf, PACKET_SIZE, 10);
    if (r < 0) return false;
    if (r < 3)  return true;  // timeout or truncated — not a hard error

    uint16_t ch = static_cast<uint16_t>((buf[1] << 4) | (buf[2] >> 4));
    channels_out.push_back(ch);
    return true;
}

}  // namespace gr1
