cmake_minimum_required(VERSION 3.8)
project(gr1)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HIDAPI REQUIRED hidapi-hidraw)

# --- Communication library (device protocol, no ROS dependency) ---
add_library(gr1_comm src/gr1_comm.cpp)
target_include_directories(gr1_comm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${HIDAPI_INCLUDE_DIRS}
)
target_link_libraries(gr1_comm ${HIDAPI_LIBRARIES})
target_compile_features(gr1_comm PUBLIC cxx_std_17)

# --- Driver library (accumulation logic, no ROS dependency) ---
add_library(gr1_driver src/gr1_driver.cpp)
target_include_directories(gr1_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(gr1_driver gr1_comm)
target_compile_features(gr1_driver PUBLIC cxx_std_17)

# --- ROS 2 driver node ---
add_executable(gamma_detector src/gr1_node.cpp)
target_include_directories(gamma_detector PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(gamma_detector gr1_driver)
target_compile_features(gamma_detector PUBLIC cxx_std_17)
ament_target_dependencies(gamma_detector rclcpp std_msgs)

# --- Install ---
install(TARGETS gr1_comm gr1_driver gamma_detector
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch/  DESTINATION share/${PROJECT_NAME}/launch)
install(DIRECTORY config/  DESTINATION share/${PROJECT_NAME}/config)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_gr1_driver test/test_gr1_driver.cpp)
  target_include_directories(test_gr1_driver PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  )
  target_link_libraries(test_gr1_driver gr1_driver)
  target_compile_features(test_gr1_driver PUBLIC cxx_std_17)
endif()

ament_package()
