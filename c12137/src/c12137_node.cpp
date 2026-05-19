// Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)
// SPDX-License-Identifier: MIT
#include "c12137/c12137_node.hpp"

#include <ament_index_cpp/get_package_share_directory.hpp>

#include <algorithm>
#include <stdexcept>

namespace c12137
{

C12137Node::C12137Node(const rclcpp::NodeOptions & options)
: rclcpp::Node("c12137", options)
{
    declare_parameter("frame_id",          std::string("c12137_1"));
    declare_parameter("ge_function_file",  std::string(""));
    declare_parameter("energy_lower_kev",  30);
    declare_parameter("energy_upper_kev",  2000);
    declare_parameter("update_interval_s", 1.0);

    std::string ge_file = get_parameter("ge_function_file").as_string();
    if (ge_file.empty()) {
        auto share = ament_index_cpp::get_package_share_directory("c12137");
        ge_file = share + "/data/gef_C12137.csv";
    }

    frame_id_          = get_parameter("frame_id").as_string();
    update_interval_s_ = get_parameter("update_interval_s").as_double();

    dose_pub_     = create_publisher<std_msgs::msg::Float64>("~/dose_rate", 10);
    temp_pub_     = create_publisher<sensor_msgs::msg::Temperature>("~/temperature", 10);
    spectrum_pub_ = create_publisher<std_msgs::msg::UInt16MultiArray>("~/spectrum", 10);

    if (!driver_.open(ge_file,
                      static_cast<uint16_t>(get_parameter("energy_lower_kev").as_int()),
                      static_cast<uint16_t>(get_parameter("energy_upper_kev").as_int())))
    {
        throw std::runtime_error(
            "Failed to open C12137 (check device connection, udev rules, and G(E) file path)");
    }

    RCLCPP_INFO(get_logger(),
                "Detector opened [frame: %s]. Energy window: %d–%d keV, interval: %.1f s",
                frame_id_.c_str(),
                get_parameter("energy_lower_kev").as_int(),
                get_parameter("energy_upper_kev").as_int(),
                update_interval_s_);

    running_     = true;
    poll_thread_ = std::thread(&C12137Node::pollLoop, this);
}

C12137Node::~C12137Node()
{
    running_ = false;
    if (poll_thread_.joinable()) poll_thread_.join();
    driver_.close();
}

void C12137Node::pollLoop()
{
    c12137::Reading r;
    while (running_) {
        if (!driver_.read(r, update_interval_s_, &running_)) continue;

        auto stamp = this->now();

        auto dose_msg = std_msgs::msg::Float64();
        dose_msg.data = r.dose_uSv_h;
        dose_pub_->publish(dose_msg);

        auto temp_msg = sensor_msgs::msg::Temperature();
        temp_msg.header.stamp    = stamp;
        temp_msg.header.frame_id = frame_id_;
        temp_msg.temperature     = r.temperature_c;
        temp_msg.variance        = 0.0;
        temp_pub_->publish(temp_msg);

        auto spec_msg = std_msgs::msg::UInt16MultiArray();
        spec_msg.data.resize(c12137::GE_TABLE_SIZE);
        for (int i = 0; i < c12137::GE_TABLE_SIZE; i++) {
            spec_msg.data[i] = static_cast<uint16_t>(
                std::min(r.spectrum[i], static_cast<uint32_t>(65535u)));
        }
        spectrum_pub_->publish(spec_msg);

        RCLCPP_INFO(get_logger(), "dose = %.4f µSv/h  |  temp = %.1f °C",
                    r.dose_uSv_h, r.temperature_c);
    }
}

}  // namespace c12137

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    try {
        rclcpp::spin(std::make_shared<c12137::C12137Node>());
    } catch (const std::exception & e) {
        RCLCPP_FATAL(rclcpp::get_logger("c12137"), "%s", e.what());
        rclcpp::shutdown();
        return 1;
    }
    rclcpp::shutdown();
    return 0;
}
