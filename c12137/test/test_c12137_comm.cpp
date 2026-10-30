// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "c12137/c12137_comm.hpp"

namespace c12137
{

// Builds a packet the way the device sends it (big-endian header words,
// little-endian event words) and loads it into host-order uint16 words exactly
// as the bulk read does.
struct WirePacket
{
    std::vector<uint8_t> bytes = std::vector<uint8_t>(BULK_TOTAL_WORDS * 2, 0);

    void putBigEndian(int word, uint16_t v)
    {
        bytes[2 * word]     = static_cast<uint8_t>(v >> 8);
        bytes[2 * word + 1] = static_cast<uint8_t>(v & 0xFF);
    }

    void putLittleEndian(int word, uint16_t v)
    {
        bytes[2 * word]     = static_cast<uint8_t>(v & 0xFF);
        bytes[2 * word + 1] = static_cast<uint8_t>(v >> 8);
    }

    std::vector<uint16_t> words() const
    {
        std::vector<uint16_t> w(BULK_TOTAL_WORDS);
        std::memcpy(w.data(), bytes.data(), bytes.size());
        return w;
    }
};

static WirePacket makePacket(uint16_t count, uint16_t index, uint16_t temp_adc,
                             const std::vector<uint16_t> & channels)
{
    WirePacket p;
    p.putBigEndian(0, 0x5A5A);
    p.putBigEndian(1, 0x5A5A);
    p.putBigEndian(2, count);
    p.putBigEndian(4, index);
    p.putBigEndian(5, temp_adc);
    for (size_t i = 0; i < channels.size(); i++) {
        // the channel sits in the top 12 bits of the event word
        p.putLittleEndian(BULK_HEADER_WORDS + static_cast<int>(i),
                          static_cast<uint16_t>(channels[i] << 4));
    }
    return p;
}

TEST(C12137DecodeTest, DecodesHeaderAndEvents)
{
    auto words = makePacket(3, 0x1234, 47030, {500, 0, 4095}).words();

    uint16_t index = 0, size = 0;
    uint16_t data[BULK_DATA_SIZE];
    double   temp = 0.0;
    ASSERT_TRUE(decodeBulkPacket(words.data(), words.size(), index, size, data, temp));

    EXPECT_EQ(index, 0x1234u);
    EXPECT_EQ(size, 3u);
    EXPECT_EQ((data[0] >> 4) & 0x0FFF, 500u);
    EXPECT_EQ((data[1] >> 4) & 0x0FFF, 0u);
    EXPECT_EQ((data[2] >> 4) & 0x0FFF, 4095u);
    EXPECT_NEAR(temp, 25.0, 0.1);
}

TEST(C12137DecodeTest, RejectsMissingSyncWords)
{
    auto packet = makePacket(1, 1, 47030, {100});
    packet.putBigEndian(1, 0x0000);
    auto words = packet.words();

    uint16_t index = 0, size = 0;
    uint16_t data[BULK_DATA_SIZE];
    double   temp = 0.0;
    EXPECT_FALSE(decodeBulkPacket(words.data(), words.size(), index, size, data, temp));
}

TEST(C12137DecodeTest, ClampsEventCountToBufferSize)
{
    auto words = makePacket(0xFFFF, 1, 47030, {}).words();

    uint16_t index = 0, size = 0;
    uint16_t data[BULK_DATA_SIZE];
    double   temp = 0.0;
    ASSERT_TRUE(decodeBulkPacket(words.data(), words.size(), index, size, data, temp));
    EXPECT_EQ(size, static_cast<uint16_t>(BULK_DATA_SIZE));
}

TEST(C12137DecodeTest, RejectsShortPackets)
{
    auto words = makePacket(1, 1, 47030, {100}).words();

    uint16_t index = 0, size = 0;
    uint16_t data[BULK_DATA_SIZE];
    double   temp = 0.0;
    EXPECT_FALSE(decodeBulkPacket(words.data(), words.size() - 1, index, size, data, temp));
    EXPECT_FALSE(decodeBulkPacket(words.data(), 0, index, size, data, temp));
}

TEST(C12137DecodeTest, ConvertsTemperatureAdc)
{
    // LM94021: 25 °C is about 897 mV, i.e. about 47030 counts of a 1250 mV, 16-bit ADC
    EXPECT_NEAR(adcToCelsius(47030), 25.0, 0.1);
    EXPECT_NEAR(adcToCelsius(54215), 0.0, 0.1);
    EXPECT_NEAR(adcToCelsius(39847), 50.0, 0.1);
}

}  // namespace c12137
