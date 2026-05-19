cmake_minimum_required(VERSION 3.8)
project(c12137)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# --- Communication library (device protocol, no ROS dependency) ---
add_library(c12137_comm src/c12137_comm.cpp)
target_include_directories(c12137_comm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${LIBUSB_INCLUDE_DIRS}
)
target_link_libraries(c12137_comm ${LIBUSB_LIBRARIES})
target_compile_features(c12137_comm PUBLIC cxx_std_17)

# --- Driver library (accumulation logic, no ROS dependency) ---
add_library(c12137_driver src/c12137_driver.cpp)
target_include_directories(c12137_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(c12137_driver c12137_comm)
target_compile_features(c12137_driver PUBLIC cxx_std_17)

# --- ROS 2 driver node ---
add_executable(gamma_detector src/c12137_node.cpp)
target_include_directories(gamma_detector PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(gamma_detector c12137_driver)
target_compile_features(gamma_detector PUBLIC cxx_std_17)
ament_target_dependencies(gamma_detector
  rclcpp
  std_msgs
  sensor_msgs
  ament_index_cpp
)

# --- Install ---
install(TARGETS c12137_comm c12137_driver gamma_detector
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY data/    DESTINATION share/${PROJECT_NAME}/data)
install(DIRECTORY launch/  DESTINATION share/${PROJECT_NAME}/launch)
install(DIRECTORY config/  DESTINATION share/${PROJECT_NAME}/config)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_c12137_driver test/test_c12137_driver.cpp)
  target_include_directories(test_c12137_driver PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  )
  target_link_libraries(test_c12137_driver c12137_driver)
  target_compile_features(test_c12137_driver PUBLIC cxx_std_17)
endif()

ament_package()
