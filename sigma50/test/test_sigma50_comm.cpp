// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "sigma50/sigma50_comm.hpp"

namespace sigma50
{

// Builds a PACKET_SIZE-byte report holding the given channels in order, followed
// by a slot with the validity bit clear (the device's end-of-list marker).
static std::vector<uint8_t> makeReport(const std::vector<uint16_t> & channels,
                                       uint8_t report_id = REPORT_SPECTRUM)
{
    std::vector<uint8_t> report(PACKET_SIZE, 0);
    report[0] = report_id;
    size_t i = 1;
    for (uint16_t ch : channels) {
        report[i]     = static_cast<uint8_t>(ch >> 4);
        report[i + 1] = static_cast<uint8_t>(((ch & 0x0F) << 4) | 0x01);
        i += 2;
    }
    return report;
}

TEST(Sigma50DecodeTest, DecodesEveryValidSlotInOrder)
{
    auto report = makeReport({100, 2000, 4095});
    std::vector<uint16_t> out;
    EXPECT_EQ(decodeReport(report.data(), report.size(), out), 3u);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0], 100u);
    EXPECT_EQ(out[1], 2000u);
    EXPECT_EQ(out[2], 4095u);
}

TEST(Sigma50DecodeTest, DecodesAFullReportOf31Events)
{
    std::vector<uint16_t> channels;
    for (uint16_t k = 0; k < 31; k++) channels.push_back(static_cast<uint16_t>(k * 100));
    auto report = makeReport(channels);
    std::vector<uint16_t> out;
    EXPECT_EQ(decodeReport(report.data(), report.size(), out), 31u);
    ASSERT_EQ(out.size(), 31u);
    EXPECT_EQ(out.back(), 3000u);
}

TEST(Sigma50DecodeTest, ChannelIsTwelveBitsSpreadOverBothBytes)
{
    std::vector<uint8_t> report(PACKET_SIZE, 0);
    report[0] = REPORT_SPECTRUM;
    report[1] = 0xAB;  // upper 8 bits of the channel
    report[2] = 0xC1;  // upper nibble = lower 4 bits of the channel, bit 0 = valid
    std::vector<uint16_t> out;
    EXPECT_EQ(decodeReport(report.data(), report.size(), out), 1u);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], 0xABCu);
}

TEST(Sigma50DecodeTest, StopsAtFirstSlotWithoutValidityBit)
{
    auto report = makeReport({10, 20, 30});
    report[4] &= 0xFE;  // clear the validity bit of the second slot
    std::vector<uint16_t> out;
    EXPECT_EQ(decodeReport(report.data(), report.size(), out), 1u);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], 10u);
}

TEST(Sigma50DecodeTest, EmptyReportYieldsNoEvents)
{
    // A report with no valid slot must not become a count in channel 0
    auto report = makeReport({});
    std::vector<uint16_t> out;
    EXPECT_EQ(decodeReport(report.data(), report.size(), out), 0u);
    EXPECT_TRUE(out.empty());
}

TEST(Sigma50DecodeTest, IgnoresReportsThatAreNotEventData)
{
    auto report = makeReport({500}, 0x81);  // a configuration reply
    std::vector<uint16_t> out;
    EXPECT_EQ(decodeReport(report.data(), report.size(), out), 0u);
    EXPECT_TRUE(out.empty());
}

TEST(Sigma50DecodeTest, IgnoresShortReads)
{
    auto report = makeReport({500});
    std::vector<uint16_t> out;
    EXPECT_EQ(decodeReport(report.data(), 0, out), 0u);                // timeout
    EXPECT_EQ(decodeReport(report.data(), PACKET_SIZE - 1, out), 0u);  // truncated
    EXPECT_TRUE(out.empty());
}

TEST(Sigma50DecodeTest, AppendsToExistingOutput)
{
    auto report = makeReport({7});
    std::vector<uint16_t> out{1, 2};
    EXPECT_EQ(decodeReport(report.data(), report.size(), out), 1u);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[2], 7u);
}

}  // namespace sigma50
