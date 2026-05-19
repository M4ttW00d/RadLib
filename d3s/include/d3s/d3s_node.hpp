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

#include "d3s/d3s_driver.hpp"

namespace d3s
{

/**
 * ROS 2 driver node for the Kromek D3S combined gamma and neutron detector.
 *
 * Published topics:
 *   ~/spectrum      (std_msgs/UInt16MultiArray)  — gamma histogram (4096 bins)
 *   ~/temperature   (sensor_msgs/Temperature)    — detector temperature in °C
 *   ~/neutron_cps   (std_msgs/Float64)           — neutron counts per second
 *
 * Parameters (see config/params.yaml):
 *   frame_id           : TF frame for this detector unit
 *   update_interval_s  : integration window length in seconds
 *   port               : serial port path; empty = auto-detect /dev/ttyACM*
 */
class D3SNode : public rclcpp::Node
{
public:
    explicit D3SNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
    ~D3SNode() override;

private:
    void pollLoop();

    d3s::D3SDriver driver_;

    rclcpp::Publisher<std_msgs::msg::UInt16MultiArray>::SharedPtr  spectrum_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Temperature>::SharedPtr    temp_pub_;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr           neutron_cps_pub_;

    std::thread       poll_thread_;
    std::atomic<bool> running_{false};

    std::string frame_id_{"d3s_1"};
    double      update_interval_s_{1.0};
};

}  // namespace d3s
