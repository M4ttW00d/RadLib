// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <vector>

#include "d3s/d3s_driver.hpp"

namespace d3s
{

struct FakeD3SDevice
{
    struct Call { bool ok; SpectrumData data; int8_t temp; };

    bool open(const std::string &) { return open_result; }
    void close() {}
    bool isOpen() const { return open_result; }

    bool getSpectrum(SpectrumData & out)
    {
        if (next < calls.size()) {
            auto & c = calls[next++];
            if (!c.ok) return false;
            out = c.data;
            return true;
        }
        out = SpectrumData{};
        return true;
    }

    bool getTemperature(int8_t & celsius_out)
    {
        celsius_out = temperature;
        return true;
    }

    bool open_result{true};
    int8_t temperature{25};
    size_t next{0};
    std::vector<Call> calls;
};

using TestDriver = D3SDriverT<FakeD3SDevice>;

static SpectrumData makeSpectrum(uint16_t ch, uint32_t count,
                                 uint16_t neutrons = 0,
                                 uint32_t scan_ms  = 1000)
{
    SpectrumData s{};
    s.channels[ch]  = count;
    s.neutron_count = neutrons;
    s.scan_time_ms  = scan_ms;
    return s;
}

// interval_s = 0.0 means the loop exits as soon as elapsed >= 0, which happens
// immediately after the first successful getSpectrum call.

TEST(D3SDriverTest, AccumulatesSpectrumFromOneScan)
{
    FakeD3SDevice fake;
    fake.calls = {{true, makeSpectrum(100, 7), 0}};
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_EQ(r.spectrum[100], 7u);
}

TEST(D3SDriverTest, ComputesNeutronCps)
{
    FakeD3SDevice fake;
    // 10 neutrons over 2000 ms = 5.0 cps
    fake.calls = {{true, makeSpectrum(0, 0, 10, 2000), 0}};
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_DOUBLE_EQ(r.neutron_cps, 5.0);
}

TEST(D3SDriverTest, NeutronCpsIsZeroWithNoScanTime)
{
    FakeD3SDevice fake;
    SpectrumData s{};
    s.scan_time_ms  = 0;
    s.neutron_count = 5;
    fake.calls = {{true, s, 0}};
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_DOUBLE_EQ(r.neutron_cps, 0.0);
}

TEST(D3SDriverTest, ReadsTemperatureAtEndOfWindow)
{
    FakeD3SDevice fake;
    fake.temperature = 35;
    fake.calls = {{true, makeSpectrum(0, 0, 0, 100), 0}};
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_DOUBLE_EQ(r.temperature_c, 35.0);
}

TEST(D3SDriverTest, ReturnsFalseWhenStopFlagSet)
{
    TestDriver drv;

    std::atomic<bool> running{false};
    Reading r;
    EXPECT_FALSE(drv.read(r, 0.0, &running));
}

TEST(D3SDriverTest, ReturnsFalseAfterRetryLimit)
{
    FakeD3SDevice fake;
    for (int i = 0; i < 5; i++)
        fake.calls.push_back({false, {}, 0});
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_FALSE(drv.read(r, 0.0));
}

TEST(D3SDriverTest, ResetsErrorCountOnSuccess)
{
    // 4 consecutive errors (below RETRY_LIMIT of 5), then success
    FakeD3SDevice fake;
    for (int i = 0; i < 4; i++)
        fake.calls.push_back({false, {}, 0});
    fake.calls.push_back({true, makeSpectrum(300, 2), 0});
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_EQ(r.spectrum[300], 2u);
}

}  // namespace d3s
