// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include "d3s/d3s_node.hpp"

#include <algorithm>
#include <stdexcept>

namespace d3s
{

D3SNode::D3SNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("d3s", options)
{
    declare_parameter("frame_id",          std::string("d3s_1"));
    declare_parameter("update_interval_s", 1.0);
    declare_parameter("port",              std::string(""));

    frame_id_          = get_parameter("frame_id").as_string();
    update_interval_s_ = get_parameter("update_interval_s").as_double();
    std::string port   = get_parameter("port").as_string();

    spectrum_pub_    = create_publisher<std_msgs::msg::UInt16MultiArray>("~/spectrum", 10);
    temp_pub_        = create_publisher<sensor_msgs::msg::Temperature>("~/temperature", 10);
    neutron_cps_pub_ = create_publisher<std_msgs::msg::Float64>("~/neutron_cps", 10);

    if (!driver_.open(port)) {
        throw std::runtime_error(
            "Failed to open Kromek D3S (is Bluetooth paired? is /dev/ttyACM* present?)");
    }

    RCLCPP_INFO(get_logger(), "D3S opened [frame: %s, interval: %.1f s]",
                frame_id_.c_str(), update_interval_s_);

    running_     = true;
    poll_thread_ = std::thread(&D3SNode::pollLoop, this);
}

D3SNode::~D3SNode()
{
    running_ = false;
    if (poll_thread_.joinable()) poll_thread_.join();
    driver_.close();
}

void D3SNode::pollLoop()
{
    d3s::Reading r;
    while (running_) {
        if (!driver_.read(r, update_interval_s_, &running_)) continue;

        auto stamp = this->now();

        auto spec_msg = std_msgs::msg::UInt16MultiArray();
        spec_msg.data.resize(d3s::SPECTRUM_BINS);
        for (int i = 0; i < d3s::SPECTRUM_BINS; i++) {
            spec_msg.data[i] = static_cast<uint16_t>(
                std::min(r.spectrum[i], static_cast<uint32_t>(65535u)));
        }
        spectrum_pub_->publish(spec_msg);

        auto ncps_msg = std_msgs::msg::Float64();
        ncps_msg.data = r.neutron_cps;
        neutron_cps_pub_->publish(ncps_msg);

        auto temp_msg = sensor_msgs::msg::Temperature();
        temp_msg.header.stamp    = stamp;
        temp_msg.header.frame_id = frame_id_;
        temp_msg.temperature     = r.temperature_c;
        temp_msg.variance        = 0.0;
        temp_pub_->publish(temp_msg);

        RCLCPP_INFO(get_logger(), "neutron_cps = %.2f  |  temp = %.0f °C",
                    r.neutron_cps, r.temperature_c);
    }
}

}  // namespace d3s

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    try {
        rclcpp::spin(std::make_shared<d3s::D3SNode>());
    } catch (const std::exception & e) {
        RCLCPP_FATAL(rclcpp::get_logger("d3s"), "%s", e.what());
        rclcpp::shutdown();
        return 1;
    }
    rclcpp::shutdown();
    return 0;
}
