cmake_minimum_required(VERSION 3.8)
project(sigma50)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HIDAPI REQUIRED hidapi-hidraw)

# --- Communication library (device protocol, no ROS dependency) ---
add_library(sigma50_comm src/sigma50_comm.cpp)
target_include_directories(sigma50_comm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${HIDAPI_INCLUDE_DIRS}
)
target_link_libraries(sigma50_comm ${HIDAPI_LIBRARIES})
target_compile_features(sigma50_comm PUBLIC cxx_std_17)

# --- Driver library (accumulation logic, no ROS dependency) ---
add_library(sigma50_driver src/sigma50_driver.cpp)
target_include_directories(sigma50_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(sigma50_driver sigma50_comm)
target_compile_features(sigma50_driver PUBLIC cxx_std_17)

# --- ROS 2 driver node ---
add_executable(gamma_detector src/sigma50_node.cpp)
target_include_directories(gamma_detector PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(gamma_detector sigma50_driver)
target_compile_features(gamma_detector PUBLIC cxx_std_17)
ament_target_dependencies(gamma_detector rclcpp std_msgs)

# --- Install ---
install(TARGETS sigma50_comm sigma50_driver gamma_detector
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch/  DESTINATION share/${PROJECT_NAME}/launch)
install(DIRECTORY config/  DESTINATION share/${PROJECT_NAME}/config)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_sigma50_driver test/test_sigma50_driver.cpp)
  target_include_directories(test_sigma50_driver PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  )
  target_link_libraries(test_sigma50_driver sigma50_driver)
  target_compile_features(test_sigma50_driver PUBLIC cxx_std_17)
endif()

ament_package()
