cmake_minimum_required(VERSION 3.16)
project(radlib_minimal_example CXX)

# Builds against an installed RadLib: cmake -DCMAKE_PREFIX_PATH=<install prefix>
find_package(radlib REQUIRED)

add_executable(radlib_minimal main.cpp)
target_link_libraries(radlib_minimal PRIVATE
  radlib::gr1_driver
  radlib::sigma50_driver
  radlib::c12137_driver
  radlib::d3s_driver
)
