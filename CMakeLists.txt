cmake_minimum_required(VERSION 3.16)
project(radlib VERSION 0.1.0)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(HIDAPI REQUIRED hidapi-hidraw)

# --- c12137 ---
add_library(c12137_comm c12137/src/c12137_comm.cpp)
target_include_directories(c12137_comm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/c12137/include>
  $<INSTALL_INTERFACE:include>
  ${LIBUSB_INCLUDE_DIRS}
)
target_link_libraries(c12137_comm ${LIBUSB_LIBRARIES})
target_compile_features(c12137_comm PUBLIC cxx_std_17)

add_library(c12137_driver c12137/src/c12137_driver.cpp)
target_include_directories(c12137_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/c12137/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(c12137_driver c12137_comm)
target_compile_features(c12137_driver PUBLIC cxx_std_17)

# --- gr1 ---
add_library(gr1_comm gr1/src/gr1_comm.cpp)
target_include_directories(gr1_comm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/gr1/include>
  $<INSTALL_INTERFACE:include>
  ${HIDAPI_INCLUDE_DIRS}
)
target_link_libraries(gr1_comm ${HIDAPI_LIBRARIES})
target_compile_features(gr1_comm PUBLIC cxx_std_17)

add_library(gr1_driver gr1/src/gr1_driver.cpp)
target_include_directories(gr1_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/gr1/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(gr1_driver gr1_comm)
target_compile_features(gr1_driver PUBLIC cxx_std_17)

# --- sigma50 ---
add_library(sigma50_comm sigma50/src/sigma50_comm.cpp)
target_include_directories(sigma50_comm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sigma50/include>
  $<INSTALL_INTERFACE:include>
  ${HIDAPI_INCLUDE_DIRS}
)
target_link_libraries(sigma50_comm ${HIDAPI_LIBRARIES})
target_compile_features(sigma50_comm PUBLIC cxx_std_17)

add_library(sigma50_driver sigma50/src/sigma50_driver.cpp)
target_include_directories(sigma50_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sigma50/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(sigma50_driver sigma50_comm)
target_compile_features(sigma50_driver PUBLIC cxx_std_17)

# --- d3s ---
add_library(d3s_comm d3s/src/d3s_comm.cpp)
target_include_directories(d3s_comm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/d3s/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(d3s_comm PUBLIC cxx_std_17)

add_library(d3s_driver d3s/src/d3s_driver.cpp)
target_include_directories(d3s_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/d3s/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(d3s_driver d3s_comm)
target_compile_features(d3s_driver PUBLIC cxx_std_17)

# --- Install ---
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS
  c12137_comm c12137_driver
  gr1_comm    gr1_driver
  sigma50_comm sigma50_driver
  d3s_comm    d3s_driver
  EXPORT radlibTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY
  c12137/include/
  gr1/include/
  sigma50/include/
  d3s/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY c12137/data/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/radlib/c12137
)

install(EXPORT radlibTargets
  FILE      radlibTargets.cmake
  NAMESPACE radlib::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/radlib
)

configure_package_config_file(
  cmake/radlibConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/radlibConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/radlib
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/radlibConfigVersion.cmake
  VERSION     ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/radlibConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/radlibConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/radlib
)
