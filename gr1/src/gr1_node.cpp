// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include "gr1/gr1_node.hpp"

#include <algorithm>
#include <stdexcept>

namespace gr1
{

GR1Node::GR1Node(const rclcpp::NodeOptions & options)
: rclcpp::Node("gr1", options)
{
    declare_parameter("frame_id",          std::string("gr1_1"));
    declare_parameter("update_interval_s", 1.0);
    declare_parameter("serial_number",     std::string(""));

    frame_id_          = get_parameter("frame_id").as_string();
    update_interval_s_ = get_parameter("update_interval_s").as_double();
    std::string serial = get_parameter("serial_number").as_string();

    cps_pub_      = create_publisher<std_msgs::msg::Float64>("~/cps", 10);
    spectrum_pub_ = create_publisher<std_msgs::msg::UInt16MultiArray>("~/spectrum", 10);

    if (!driver_.open(serial)) {
        throw std::runtime_error(
            "Failed to open Kromek GR1 (is it plugged in? check udev rules)");
    }

    RCLCPP_INFO(get_logger(), "GR1 opened [frame: %s, interval: %.1f s]",
                frame_id_.c_str(), update_interval_s_);

    running_     = true;
    poll_thread_ = std::thread(&GR1Node::pollLoop, this);
}

GR1Node::~GR1Node()
{
    running_ = false;
    if (poll_thread_.joinable()) poll_thread_.join();
    driver_.close();
}

void GR1Node::pollLoop()
{
    gr1::Reading r;
    while (running_) {
        if (!driver_.read(r, update_interval_s_, &running_)) continue;

        auto cps_msg = std_msgs::msg::Float64();
        cps_msg.data = r.cps;
        cps_pub_->publish(cps_msg);

        auto spec_msg = std_msgs::msg::UInt16MultiArray();
        spec_msg.data.resize(gr1::SPECTRUM_BINS);
        for (int i = 0; i < gr1::SPECTRUM_BINS; i++) {
            spec_msg.data[i] = static_cast<uint16_t>(
                std::min(r.spectrum[i], static_cast<uint32_t>(65535u)));
        }
        spectrum_pub_->publish(spec_msg);

        RCLCPP_INFO(get_logger(), "cps = %.1f", r.cps);
    }
}

}  // namespace gr1

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    try {
        rclcpp::spin(std::make_shared<gr1::GR1Node>());
    } catch (const std::exception & e) {
        RCLCPP_FATAL(rclcpp::get_logger("gr1"), "%s", e.what());
        rclcpp::shutdown();
        return 1;
    }
    rclcpp::shutdown();
    return 0;
}
