// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <atomic>
#include <vector>

#include "sigma50/sigma50_driver.hpp"

namespace sigma50
{

struct FakeSigma50Device
{
    struct Call { bool ok; std::vector<uint16_t> events; };

    bool open(const std::string &) { return open_result; }
    void close() {}
    bool isOpen() const { return open_result; }

    bool readEvents(std::vector<uint16_t> & out)
    {
        if (next < calls.size()) {
            auto & c = calls[next++];
            if (c.ok) out = c.events;
            return c.ok;
        }
        out.clear();
        return true;
    }

    bool open_result{true};
    size_t next{0};
    std::vector<Call> calls;
};

using TestDriver = Sigma50DriverT<FakeSigma50Device>;

TEST(Sigma50DriverTest, AccumulatesEventsIntoSpectrum)
{
    FakeSigma50Device fake;
    fake.calls = {{true, {300, 1000, 300}}};
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_EQ(r.spectrum[300], 2u);
    EXPECT_EQ(r.spectrum[1000], 1u);
}

TEST(Sigma50DriverTest, IgnoresOutOfRangeChannels)
{
    FakeSigma50Device fake;
    fake.calls = {{true, {500, static_cast<uint16_t>(SPECTRUM_BINS)}}};
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_EQ(r.spectrum[500], 1u);

    uint64_t total = 0;
    for (auto c : r.spectrum) total += c;
    EXPECT_EQ(total, 1u);
}

TEST(Sigma50DriverTest, ReturnsFalseWhenStopFlagSet)
{
    TestDriver drv;

    std::atomic<bool> running{false};
    Reading r;
    EXPECT_FALSE(drv.read(r, 0.0, &running));
}

TEST(Sigma50DriverTest, ReturnsFalseAfterRetryLimit)
{
    FakeSigma50Device fake;
    for (int i = 0; i < 5; i++)
        fake.calls.push_back({false, {}});
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_FALSE(drv.read(r, 0.0));
}

TEST(Sigma50DriverTest, ResetsErrorCountOnSuccess)
{
    FakeSigma50Device fake;
    for (int i = 0; i < 4; i++)
        fake.calls.push_back({false, {}});
    fake.calls.push_back({true, {2000}});
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_EQ(r.spectrum[2000], 1u);
}

}  // namespace sigma50
