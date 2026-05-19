// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "c12137/c12137_driver.hpp"

namespace c12137
{

// Writes a minimal G(E) CSV: 4096 rows of "i,1.0"
// ge_energy_[i] = i keV, ge_factor_[i] = 1.0 pSv/count.
static std::string makeGeCsv()
{
    std::ostringstream ss;
    for (int i = 0; i < GE_TABLE_SIZE; i++)
        ss << i << ",1.0\n";
    return ss.str();
}

static const std::string kTmpGe = "/tmp/test_c12137_ge.csv";

static void writeTmpGe(const std::string & csv)
{
    std::ofstream f(kTmpGe);
    f << csv;
}

struct FakeC12137Device
{
    struct Call { bool ok; uint16_t new_index; uint16_t size; std::vector<uint16_t> buf; double temp; };

    bool open() { return open_result; }
    void close() {}
    bool isOpen() const { return open_result; }
    void clearBulkBuffer() {}

    bool getDataAndTemperature(uint16_t & new_index, uint16_t & size,
                               uint16_t * event_buf, double & temperature)
    {
        if (next < calls.size()) {
            auto & c = calls[next++];
            if (!c.ok) return false;
            new_index   = c.new_index;
            size        = c.size;
            temperature = c.temp;
            for (uint16_t i = 0; i < c.size; i++)
                event_buf[i] = c.buf[i];
            return true;
        }
        // Drain: return new index each call so the loop can complete
        new_index   = auto_index_++;
        size        = 0;
        temperature = 20.0;
        return true;
    }

    bool open_result{true};
    size_t next{0};
    uint16_t auto_index_{1};
    std::vector<Call> calls;
};

using TestDriver = C12137DriverT<FakeC12137Device>;

TEST(C12137DriverTest, OpenFailsWithMissingGeFile)
{
    TestDriver drv;
    EXPECT_FALSE(drv.open("/tmp/does_not_exist_c12137.csv"));
}

TEST(C12137DriverTest, AccumulatesEventsAndComputesDose)
{
    writeTmpGe(makeGeCsv());

    // addr=500, encoded: addr = (buf >> 4) & 0x0FFF, so buf = addr << 4
    const uint16_t addr    = 500;
    const uint16_t encoded = static_cast<uint16_t>(addr << 4);

    FakeC12137Device fake;
    // One packet: new_index=1 (different from old_index=0), one event at channel 500
    fake.calls = {{true, 1, 1, {encoded}, 22.5}};

    TestDriver drv(std::move(fake));
    // Use full channel range so addr=500 falls within (lower=0, upper=4095)
    ASSERT_TRUE(drv.open(kTmpGe, 0, GE_TABLE_SIZE - 1));

    Reading r;
    // interval_s=0.1 -> target_ticks=1; exits after one collected packet
    EXPECT_TRUE(drv.read(r, 0.1));
    EXPECT_EQ(r.spectrum[addr], 1u);
    EXPECT_GT(r.dose_uSv_h, 0.0);
    EXPECT_DOUBLE_EQ(r.temperature_c, 22.5);
}

TEST(C12137DriverTest, SkipsDuplicatePacketIndex)
{
    writeTmpGe(makeGeCsv());

    const uint16_t addr    = 200;
    const uint16_t encoded = static_cast<uint16_t>(addr << 4);

    FakeC12137Device fake;
    // index=1 (new), one event -> collected=1
    // index=1 again (duplicate) -> skipped
    // index=2 (new), no events -> collected=2 -> exits (target_ticks=2)
    fake.calls = {
        {true, 1, 1, {encoded}, 20.0},
        {true, 1, 1, {encoded}, 20.0},
        {true, 2, 0, {},         20.0},
    };

    TestDriver drv(std::move(fake));
    ASSERT_TRUE(drv.open(kTmpGe, 0, GE_TABLE_SIZE - 1));

    Reading r;
    EXPECT_TRUE(drv.read(r, 0.2));
    // Only the first packet's event should appear; duplicate was skipped
    EXPECT_EQ(r.spectrum[addr], 1u);
}

TEST(C12137DriverTest, ReturnsFalseWhenStopFlagSet)
{
    writeTmpGe(makeGeCsv());

    TestDriver drv;
    ASSERT_TRUE(drv.open(kTmpGe, 0, GE_TABLE_SIZE - 1));

    std::atomic<bool> running{false};
    Reading r;
    // Large interval so target_ticks > 0; stop flag fires before loop body
    EXPECT_FALSE(drv.read(r, 1.0, &running));
}

TEST(C12137DriverTest, ReturnsFalseAfterRetryLimit)
{
    writeTmpGe(makeGeCsv());

    FakeC12137Device fake;
    for (int i = 0; i < 5; i++)
        fake.calls.push_back({false, 0, 0, {}, 0.0});

    TestDriver drv(std::move(fake));
    ASSERT_TRUE(drv.open(kTmpGe, 0, GE_TABLE_SIZE - 1));

    Reading r;
    EXPECT_FALSE(drv.read(r, 0.1));
}

}  // namespace c12137
