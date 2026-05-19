// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/temperature.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/u_int16_multi_array.hpp>

#include "c12137/c12137_driver.hpp"

namespace c12137
{

/**
 * ROS 2 driver node for the Hamamatsu C12137 radiation detector.
 *
 * Published topics:
 *   ~/dose_rate   (std_msgs/Float64)          — air dose rate in µSv/h
 *   ~/temperature (sensor_msgs/Temperature)   — detector temperature in °C
 *   ~/spectrum    (std_msgs/UInt16MultiArray)  — raw event histogram (4096 bins)
 *
 * Parameters (see config/params.yaml):
 *   ge_function_file    : path to the G(E) CSV; empty = use package default
 *   energy_lower_kev    : lower bound of dose integration window (keV)
 *   energy_upper_kev    : upper bound of dose integration window (keV)
 *   update_interval_s   : how many seconds of data to integrate per reading
 */
class C12137Node : public rclcpp::Node
{
public:
    explicit C12137Node(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
    ~C12137Node() override;

private:
    void pollLoop();

    c12137::C12137Driver driver_;

    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr           dose_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Temperature>::SharedPtr    temp_pub_;
    rclcpp::Publisher<std_msgs::msg::UInt16MultiArray>::SharedPtr  spectrum_pub_;

    std::thread       poll_thread_;
    std::atomic<bool> running_{false};

    std::string frame_id_{"c12137_1"};
    double      update_interval_s_{1.0};
};

}  // namespace c12137
