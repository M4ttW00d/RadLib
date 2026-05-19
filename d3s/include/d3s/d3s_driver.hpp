// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

#include "d3s/d3s_comm.hpp"

namespace d3s
{

struct Reading
{
    std::array<uint32_t, SPECTRUM_BINS> spectrum{};
    double neutron_cps{0.0};
    double temperature_c{0.0};
    double elapsed_s{0.0};
};

template<typename TDevice = D3SDevice>
class D3SDriverT
{
public:
    D3SDriverT() = default;
    explicit D3SDriverT(TDevice device) : device_(std::move(device)) {}

    bool open(const std::string & port = "") { return device_.open(port); }
    void close() { device_.close(); }
    bool isOpen() const { return device_.isOpen(); }

    bool read(Reading & out, double interval_s,
              const std::atomic<bool> * running = nullptr)
    {
        constexpr int RETRY_LIMIT = 5;

        out.spectrum.fill(0);
        uint64_t total_neutrons = 0;
        uint64_t total_scan_ms  = 0;
        int      err_count      = 0;
        auto     window_start   = std::chrono::steady_clock::now();

        while (true) {
            if (running && !running->load()) return false;

            SpectrumData scan{};
            if (!device_.getSpectrum(scan)) {
                if (++err_count >= RETRY_LIMIT) return false;
                continue;
            }
            err_count = 0;

            for (int i = 0; i < SPECTRUM_BINS; i++) {
                out.spectrum[i] += scan.channels[i];
            }
            total_neutrons += scan.neutron_count;
            total_scan_ms  += scan.scan_time_ms;

            auto   now     = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - window_start).count();
            if (elapsed < interval_s) continue;

            out.neutron_cps = (total_scan_ms > 0)
                ? static_cast<double>(total_neutrons) / (static_cast<double>(total_scan_ms) / 1000.0)
                : 0.0;
            out.elapsed_s = elapsed;

            int8_t temp_c = 0;
            device_.getTemperature(temp_c);
            out.temperature_c = static_cast<double>(temp_c);

            return true;
        }
    }

private:
    TDevice device_;
};

using D3SDriver = D3SDriverT<>;

}  // namespace d3s
