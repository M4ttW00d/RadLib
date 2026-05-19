// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <atomic>
#include <vector>

#include "gr1/gr1_driver.hpp"

namespace gr1
{

struct FakeGR1Device
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

using TestDriver = GR1DriverT<FakeGR1Device>;

TEST(GR1DriverTest, AccumulatesEventsIntoSpectrum)
{
    FakeGR1Device fake;
    fake.calls = {{true, {100, 200, 100}}};
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_EQ(r.spectrum[100], 2u);
    EXPECT_EQ(r.spectrum[200], 1u);
}

TEST(GR1DriverTest, IgnoresOutOfRangeChannels)
{
    FakeGR1Device fake;
    // SPECTRUM_BINS == 4096; channel 4096 is out of range
    fake.calls = {{true, {500, static_cast<uint16_t>(SPECTRUM_BINS)}}};
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_EQ(r.spectrum[500], 1u);

    uint64_t total = 0;
    for (auto c : r.spectrum) total += c;
    EXPECT_EQ(total, 1u);
}

TEST(GR1DriverTest, ReturnsFalseWhenStopFlagSet)
{
    TestDriver drv;

    std::atomic<bool> running{false};
    Reading r;
    EXPECT_FALSE(drv.read(r, 0.0, &running));
}

TEST(GR1DriverTest, ReturnsFalseAfterRetryLimit)
{
    FakeGR1Device fake;
    for (int i = 0; i < 5; i++)
        fake.calls.push_back({false, {}});
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_FALSE(drv.read(r, 0.0));
}

TEST(GR1DriverTest, ResetsErrorCountOnSuccess)
{
    // 4 consecutive errors (below RETRY_LIMIT of 5), then a success
    FakeGR1Device fake;
    for (int i = 0; i < 4; i++)
        fake.calls.push_back({false, {}});
    fake.calls.push_back({true, {750}});
    TestDriver drv(std::move(fake));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.0));
    EXPECT_EQ(r.spectrum[750], 1u);
}

}  // namespace gr1
