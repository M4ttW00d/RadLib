cmake_minimum_required(VERSION 3.16)
project(d3s)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

# --- Communication library (device protocol, no ROS dependency) ---
add_library(d3s_comm src/d3s_comm.cpp)
target_include_directories(d3s_comm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(d3s_comm PUBLIC cxx_std_17)

# --- Driver library (accumulation logic, no ROS dependency) ---
add_library(d3s_driver src/d3s_driver.cpp)
target_include_directories(d3s_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(d3s_driver d3s_comm)
target_compile_features(d3s_driver PUBLIC cxx_std_17)

# --- ROS 2 driver node ---
add_executable(gamma_neutron_detector src/d3s_node.cpp)
target_include_directories(gamma_neutron_detector PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(gamma_neutron_detector d3s_driver)
target_compile_features(gamma_neutron_detector PUBLIC cxx_std_17)
ament_target_dependencies(gamma_neutron_detector rclcpp std_msgs sensor_msgs)

# --- Install ---
install(TARGETS d3s_comm d3s_driver gamma_neutron_detector
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch/  DESTINATION share/${PROJECT_NAME}/launch)
install(DIRECTORY config/  DESTINATION share/${PROJECT_NAME}/config)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_d3s_driver test/test_d3s_driver.cpp)
  target_include_directories(test_d3s_driver PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  )
  target_link_libraries(test_d3s_driver d3s_driver)
  target_compile_features(test_d3s_driver PUBLIC cxx_std_17)
endif()

ament_package()
