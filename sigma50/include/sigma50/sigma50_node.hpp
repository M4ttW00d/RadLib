// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/u_int16_multi_array.hpp>

#include "sigma50/sigma50_driver.hpp"

namespace sigma50
{

/**
 * ROS 2 driver node for the Kromek Sigma 50 gamma-ray detector.
 *
 * Published topics:
 *   ~/spectrum   (std_msgs/UInt16MultiArray)  — raw event histogram (4096 bins)
 *   ~/cps        (std_msgs/Float64)           — total counts per second
 *
 * Parameters (see config/params.yaml):
 *   frame_id           : TF frame for this detector unit
 *   update_interval_s  : integration window length in seconds
 *   serial_number      : HID serial number string; empty = any Sigma 50
 */
class Sigma50Node : public rclcpp::Node
{
public:
    explicit Sigma50Node(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
    ~Sigma50Node() override;

private:
    void pollLoop();

    sigma50::Sigma50Driver driver_;

    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr          cps_pub_;
    rclcpp::Publisher<std_msgs::msg::UInt16MultiArray>::SharedPtr spectrum_pub_;

    std::thread       poll_thread_;
    std::atomic<bool> running_{false};

    std::string frame_id_{"sigma50_1"};
    double      update_interval_s_{1.0};
};

}  // namespace sigma50
