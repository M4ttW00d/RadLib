// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <istream>
#include <sstream>
#include <string>
#include <thread>

#include "c12137/c12137_comm.hpp"

namespace c12137
{

struct Reading
{
    std::array<uint32_t, GE_TABLE_SIZE> spectrum{};
    double dose_uSv_h{0.0};
    double temperature_c{0.0};
};

template<typename TDevice = C12137Device>
class C12137DriverT
{
public:
    C12137DriverT() = default;
    explicit C12137DriverT(TDevice device) : device_(std::move(device)) {}

    // ge_file: path to the G(E) CSV. energy_lower/upper_kev: dose integration window.
    bool open(const std::string & ge_file,
              uint16_t energy_lower_kev = 30,
              uint16_t energy_upper_kev = 2000)
    {
        std::ifstream f(ge_file);
        if (!f.is_open()) return false;
        if (!loadGeFunction(f)) return false;
        lower_index_ = keVToIndex(energy_lower_kev);
        upper_index_ = keVToIndex(energy_upper_kev);
        return device_.open();
    }

    void close() { device_.close(); }
    bool isOpen() const { return device_.isOpen(); }

    bool read(Reading & out, double interval_s,
              const std::atomic<bool> * running = nullptr)
    {
        constexpr int POLL_INTERVAL_US = 20'000;
        constexpr int RETRY_LIMIT      = 5;

        out.spectrum.fill(0);

        int      target_ticks = static_cast<int>(interval_s * 10.0);
        int      collected    = 0;
        int      err_count    = 0;
        uint16_t old_index    = 0;
        double   last_temp    = 0.0;

        device_.clearBulkBuffer();

        while (collected < target_ticks) {
            if (running && !running->load()) return false;

            std::this_thread::sleep_for(std::chrono::microseconds(POLL_INTERVAL_US));

            uint16_t new_index = 0, size = 0;
            double   temperature = 0.0;
            uint16_t event_buf[BULK_DATA_SIZE];

            if (!device_.getDataAndTemperature(new_index, size, event_buf, temperature)) {
                if (++err_count >= RETRY_LIMIT) return false;
                device_.clearBulkBuffer();
                continue;
            }
            err_count = 0;
            last_temp = temperature;

            if (new_index == old_index) continue;
            old_index = new_index;

            for (uint16_t i = 0; i < size; i++) {
                uint16_t addr = (event_buf[i] >> 4) & 0x0FFF;
                if (addr > lower_index_ && addr < upper_index_) {
                    out.spectrum[addr]++;
                }
            }
            collected++;
        }

        if (running && !running->load()) return false;

        double sv_pSv = 0.0;
        for (int i = 0; i < GE_TABLE_SIZE; i++) {
            sv_pSv += static_cast<double>(out.spectrum[i]) * ge_factor_[i];
        }
        out.dose_uSv_h    = sv_pSv * 3600.0 / interval_s / 1.0e6;
        out.temperature_c = last_temp;

        return true;
    }

private:
    bool loadGeFunction(std::istream & stream)
    {
        std::string line;
        for (int i = 0; i < GE_TABLE_SIZE; i++) {
            if (!std::getline(stream, line)) return false;
            std::replace(line.begin(), line.end(), ',', ' ');
            std::istringstream ss(line);
            if (!(ss >> ge_energy_[i] >> ge_factor_[i])) return false;
        }
        return true;
    }

    uint16_t keVToIndex(uint16_t kev) const
    {
        for (int n = 0; n < GE_TABLE_SIZE; n++) {
            if (ge_energy_[n] >= static_cast<double>(kev)) {
                return static_cast<uint16_t>(n);
            }
        }
        return static_cast<uint16_t>(GE_TABLE_SIZE - 1);
    }

    TDevice device_;
    std::array<double, GE_TABLE_SIZE> ge_energy_{};
    std::array<double, GE_TABLE_SIZE> ge_factor_{};
    uint16_t lower_index_{0};
    uint16_t upper_index_{static_cast<uint16_t>(GE_TABLE_SIZE - 1)};
};

using C12137Driver = C12137DriverT<>;

}  // namespace c12137
