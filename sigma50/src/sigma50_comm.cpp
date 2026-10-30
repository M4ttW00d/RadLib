// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include "sigma50/sigma50_comm.hpp"

namespace sigma50
{

size_t decodeReport(const uint8_t * report, size_t length,
                    std::vector<uint16_t> & channels_out)
{
    if (length < static_cast<size_t>(PACKET_SIZE) || report[0] != REPORT_SPECTRUM) {
        return 0;
    }

    size_t count = 0;
    for (int i = 1; i + 1 < PACKET_SIZE; i += 2) {
        const uint8_t byte1 = report[i];
        const uint8_t byte2 = report[i + 1];
        if (!(byte2 & 0x01)) break;  // validity bit clear: end of the event list
        channels_out.push_back(static_cast<uint16_t>((byte1 << 4) | (byte2 >> 4)));
        ++count;
    }
    return count;
}

// hidapi keeps a single process-wide context shared by every open device, so
// hid_exit() is deliberately never called here: calling it from a destructor
// tears that context down under any other device still open in the process
// (harmless on the hidraw backend, fatal on the libusb and macOS backends).
// The small amount of static state is released when the process exits.
Sigma50Device::Sigma50Device()  { hid_init(); }
Sigma50Device::~Sigma50Device() { close(); }

bool Sigma50Device::open(const std::string & serial_number)
{
    close();

    if (serial_number.empty()) {
        dev_ = hid_open(VENDOR_ID, PRODUCT_ID, nullptr);
        return dev_ != nullptr;
    }

    // hid_open(vid, pid, serial) in older hidapi releases (0.11.2 as shipped by
    // Ubuntu 22.04) compares against each candidate's serial string without a
    // null check and crashes if any matching unit reports none. Match here
    // instead and open the chosen unit by path.
    const std::wstring wanted(serial_number.begin(), serial_number.end());
    hid_device_info * list = hid_enumerate(VENDOR_ID, PRODUCT_ID);
    for (const hid_device_info * d = list; d != nullptr; d = d->next) {
        if (d->serial_number != nullptr && wanted == d->serial_number) {
            dev_ = hid_open_path(d->path);
            break;
        }
    }
    hid_free_enumeration(list);
    return dev_ != nullptr;
}

void Sigma50Device::close()
{
    if (dev_) { hid_close(dev_); dev_ = nullptr; }
}

bool Sigma50Device::readEvents(std::vector<uint16_t> & channels_out)
{
    channels_out.clear();
    if (!dev_) return false;

    uint8_t buf[PACKET_SIZE] = {};

    // Block up to 10 ms; returns 0 on timeout, -1 on error
    int r = hid_read_timeout(dev_, buf, PACKET_SIZE, 10);
    if (r < 0) return false;

    decodeReport(buf, static_cast<size_t>(r), channels_out);
    return true;
}

}  // namespace sigma50
