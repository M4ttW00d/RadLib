// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "gr1/gr1_comm.hpp"

namespace gr1
{

struct Reading
{
    std::array<uint32_t, SPECTRUM_BINS> spectrum{};
    double cps{0.0};
    double elapsed_s{0.0};
};

template<typename TDevice = GR1Device>
class GR1DriverT
{
public:
    GR1DriverT() = default;
    explicit GR1DriverT(TDevice device) : device_(std::move(device)) {}

    bool open(const std::string & serial = "") { return device_.open(serial); }
    void close() { device_.close(); }
    bool isOpen() const { return device_.isOpen(); }

    bool read(Reading & out, double interval_s,
              const std::atomic<bool> * running = nullptr)
    {
        constexpr int RETRY_LIMIT = 5;

        out.spectrum.fill(0);
        uint64_t total_counts = 0;
        int      err_count    = 0;
        auto     window_start = std::chrono::steady_clock::now();

        while (true) {
            if (running && !running->load()) return false;

            std::vector<uint16_t> events;
            if (!device_.readEvents(events)) {
                if (++err_count >= RETRY_LIMIT) return false;
                continue;
            }
            err_count = 0;

            for (uint16_t ch : events) {
                if (ch < SPECTRUM_BINS) {
                    out.spectrum[ch]++;
                    total_counts++;
                }
            }

            auto   now     = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - window_start).count();
            if (elapsed < interval_s) continue;

            out.cps       = static_cast<double>(total_counts) / elapsed;
            out.elapsed_s = elapsed;
            return true;
        }
    }

private:
    TDevice device_;
};

using GR1Driver = GR1DriverT<>;

}  // namespace gr1
