// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "d3s/d3s_comm.hpp"

namespace d3s
{

static void putLE16(std::vector<uint8_t> & f, size_t off, uint16_t v)
{
    f[off]     = static_cast<uint8_t>(v & 0xFF);
    f[off + 1] = static_cast<uint8_t>(v >> 8);
}

static void putLE32(std::vector<uint8_t> & f, size_t off, uint32_t v)
{
    for (int i = 0; i < 4; i++) f[off + i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
}

TEST(D3SDecodeTest, DecodesSpectrumFrame)
{
    std::vector<uint8_t> f(SPECTRUM_PACKET, 0);
    putLE16(f, 0, SPECTRUM_PACKET);
    f[4] = 0xC1;
    putLE32(f, 5, 1500);                 // scan_time_ms
    putLE16(f, 9, 7);                    // neutron_count
    putLE16(f, 11 + 2 * 100, 42);        // channel 100
    putLE16(f, 11 + 2 * 4095, 65535);    // last channel

    SpectrumData d{};
    ASSERT_TRUE(decodeSpectrumFrame(f.data(), f.size(), d));
    EXPECT_EQ(d.scan_time_ms, 1500u);
    EXPECT_EQ(d.neutron_count, 7u);
    EXPECT_EQ(d.channels[0], 0u);
    EXPECT_EQ(d.channels[100], 42u);
    EXPECT_EQ(d.channels[4095], 65535u);
}

TEST(D3SDecodeTest, RejectsShortSpectrumFrame)
{
    std::vector<uint8_t> f(SPECTRUM_PACKET, 0);
    SpectrumData d{};
    EXPECT_FALSE(decodeSpectrumFrame(f.data(), SPECTRUM_PACKET - 1, d));
    EXPECT_FALSE(decodeSpectrumFrame(f.data(), 0, d));
}

TEST(D3SDecodeTest, DecodesSignedStatusTemperature)
{
    std::vector<uint8_t> f(STATUS_PACKET, 0);
    int8_t t = 0;

    f[7] = 35;
    ASSERT_TRUE(decodeStatusFrame(f.data(), f.size(), t));
    EXPECT_EQ(t, 35);

    f[7] = static_cast<uint8_t>(-12);
    ASSERT_TRUE(decodeStatusFrame(f.data(), f.size(), t));
    EXPECT_EQ(t, -12);
}

TEST(D3SDecodeTest, RejectsShortStatusFrame)
{
    std::vector<uint8_t> f(STATUS_PACKET, 0);
    int8_t t = 0;
    EXPECT_FALSE(decodeStatusFrame(f.data(), STATUS_PACKET - 1, t));
}

TEST(D3SDecodeTest, DecodesSerialNumber)
{
    std::vector<uint8_t> f(ABOUT_PACKET, 0);
    std::memcpy(f.data() + 59, "SGM123456XYZ", 12);  // only the first 9 chars are the serial

    std::string s;
    ASSERT_TRUE(decodeAboutFrame(f.data(), f.size(), s));
    EXPECT_EQ(s, "SGM123456");
}

TEST(D3SDecodeTest, SerialNumberStopsAtNul)
{
    std::vector<uint8_t> f(ABOUT_PACKET, 0);
    std::memcpy(f.data() + 59, "SGM12", 5);

    std::string s;
    ASSERT_TRUE(decodeAboutFrame(f.data(), f.size(), s));
    EXPECT_EQ(s, "SGM12");
}

TEST(D3SDecodeTest, RejectsShortAboutFrame)
{
    std::vector<uint8_t> f(ABOUT_PACKET, 0);
    std::string s;
    EXPECT_FALSE(decodeAboutFrame(f.data(), ABOUT_PACKET - 1, s));
}

}  // namespace d3s
