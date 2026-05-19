# RadLib (Radiation Library)

[![CI](https://github.com/M4ttW00d/RadLib/actions/workflows/ci.yml/badge.svg)](https://github.com/M4ttW00d/RadLib/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> **This project is still in development.** The drivers work against real hardware but the codebase has not been formally tested or audited. There will be bugs. Use with appropriate caution and please report issues.

RadLib is a C++ library of hardware drivers for radiation detectors. It provides a clean, layered interface to a set of USB and Bluetooth radiation sensors, with optional ROS 2 integration. The driver layer has no ROS dependency and can be used in any C++ application directly.

The library is designed for use in robotics and automated survey systems where detectors need to be read continuously, integrated over configurable time windows, and their output consumed either by a ROS 2 node or directly by application code.

---

## Contents

- [Supported Hardware](#supported-hardware)
- [Architecture](#architecture)
- [Dependencies](#dependencies)
- [Platform Support](#platform-support)
- [Building with ROS 2](#building-with-ros-2)
- [Usage with ROS 2](#usage-with-ros-2)
- [Usage without ROS 2](#usage-without-ros-2)
- [Contributing](#contributing)
- [Acknowledgements](#acknowledgements)
- [License](#license)

---

## Supported Hardware

### Hamamatsu C12137

A compact gamma-ray detection module combining a scintillator crystal with an MPPC (Multi-Pixel Photon Counter) silicon photodetector. Onboard signal processing and A/D conversion produce a 4096-channel energy spectrum over USB. RadLib computes air dose rate in µSv/h from the spectrum using a G(E) conversion factor table bundled with the package.

- Connection: USB bulk transfer (libusb-1.0), VID/PID `0x0661` / `0x2917`
- Outputs: energy spectrum, dose rate (µSv/h), detector temperature

### Kromek GR1

A handheld gamma-ray spectrometer using a 1 cm³ coplanar-grid CZT (Cadmium Zinc Telluride) detector. Covers 30 keV to 3 MeV with an energy resolution of approximately 2.0-2.5% FWHM at 662 keV, significantly better than scintillator-based alternatives of similar size. Bus-powered over USB.

- Connection: USB HID (hidapi-hidraw), VID/PID `0x04D8` / `0x0000`
- Outputs: energy spectrum, counts per second

### Kromek Sigma 50

A portable gamma spectrometer using a 1" x 1" x 2" CsI (caesium iodide) scintillator (32.9 cm³ active volume). Covers 50 keV to 2 MeV. Larger active volume than the GR1 gives higher detection efficiency, making it well suited to environmental monitoring and field surveys where sensitivity matters more than energy resolution.

- Connection: USB HID (hidapi-hidraw), VID/PID `0x04D8` / `0x0023`
- Outputs: energy spectrum, counts per second

### Kromek D3S

A wearable personal radiation detector combining a CsI(Tl) gamma spectrometer (30 keV to 3 MeV) with a compact thermal neutron scintillator for simultaneous dual-particle detection. Designed for first responders and networked radiation monitoring. Connects wirelessly via Bluetooth SPP, appearing as a serial port on the host system.

- Connection: Bluetooth SPP at `/dev/ttyACM*`, 115200 baud
- Outputs: gamma spectrum, neutron counts per second, detector temperature
- Auto-detects the first `/dev/ttyACM*` that identifies as a D3S (serial number prefix `SGM`)

---

## Architecture

Each detector package has three layers:

```
comm      Raw device I/O over USB, HID, or serial. No ROS dependency.
          One method per device command, returns bool on success/failure.

driver    Accumulates raw events into a histogram over a configurable
          time window, then returns a plain C++ Reading struct.
          No ROS dependency. Use this layer to integrate detectors into
          any C++ application.

node      Thin ROS 2 wrapper. Calls driver.read() in a background thread
          and publishes the result to ROS topics. Only this layer
          requires ROS.
```

The comm and driver layers are built as standalone libraries (`c12137_comm`, `c12137_driver`, etc.) with no ROS headers or link dependencies. The node layer is the only part that requires a ROS 2 installation.

---

## Dependencies

**System libraries:**

| Package | Requires |
|---|---|
| `c12137` | `libusb-1.0` |
| `gr1` | `libhidapi-hidraw` |
| `sigma50` | `libhidapi-hidraw` |
| `d3s` | none (POSIX serial) |

**ROS 2 packages** (node layer only): `rclcpp`, `std_msgs`, `sensor_msgs` (c12137 and d3s), `ament_index_cpp` (c12137).

---

## Platform Support

RadLib is developed and tested on Linux. The sections below describe the current state on other platforms and what would be needed to support them fully.

### Linux

Fully supported. All four packages build and run as described in this document.

### macOS

Three of the four packages (c12137, gr1, sigma50) should work on macOS with one build change: `hidapi-hidraw` is the Linux-specific HID backend and needs to be replaced with `hidapi` (which uses macOS IOKit) in the CMakeLists files for `gr1` and `sigma50`. libusb, used by `c12137`, is available via Homebrew (`brew install libusb`) and works without code changes.

The `d3s` package needs a small change to port auto-detection. On macOS the D3S Bluetooth serial device appears as `/dev/cu.usbmodem*` rather than `/dev/ttyACM*`, so the detection logic in `d3s_comm.cpp` would need updating to scan for that pattern instead.

### Windows

The `d3s` package uses `termios.h` for serial port configuration, which is a POSIX interface not available on Windows. Bringing `d3s` to Windows would require replacing the serial layer with either Win32 serial API calls or a cross-platform serial library. The other three packages would need the same HID backend change as macOS, and libusb has Windows support via WinUSB.

Windows is not a current priority but contributions are welcome. If ROS 2 integration is not required, the driver layer alone is a smaller porting target since it has no ROS dependencies.

---

## Building with ROS 2

Place the packages in a ROS 2 workspace and build with colcon:

```bash
cd ~/your_ws
colcon build --packages-select c12137 gr1 sigma50 d3s
source install/setup.bash
```

---

## Usage with ROS 2

Launch a single detector with its default parameters:

```bash
ros2 launch gr1 gr1.launch.py
ros2 launch sigma50 sigma50.launch.py
ros2 launch c12137 c12137.launch.py
ros2 launch d3s d3s.launch.py
```

Namespaces are not set in the launch files. Provide them at the robot or fleet level:

```bash
ros2 launch gr1 gr1.launch.py __ns:=/robot1
```

Or from a parent launch file:

```python
IncludeLaunchDescription(
    PythonLaunchDescriptionSource([FindPackageShare("gr1"), "/launch/gr1.launch.py"]),
    launch_arguments={"__ns": "/robot1"}.items(),
)
```

### Parameters

All packages expose `frame_id` and `update_interval_s`. Edit `config/params.yaml` in the package or override at launch.

**`c12137`**

| Parameter | Default | Description |
|---|---|---|
| `frame_id` | `c12137_1` | TF frame name for this detector unit |
| `update_interval_s` | `1.0` | Integration window length in seconds |
| `ge_function_file` | *(bundled)* | Path to G(E) CSV; empty uses the file bundled with the package |
| `energy_lower_kev` | `30` | Lower bound of dose rate integration window (keV) |
| `energy_upper_kev` | `2000` | Upper bound of dose rate integration window (keV) |

**`gr1` and `sigma50`**

| Parameter | Default | Description |
|---|---|---|
| `frame_id` | `gr1_1` / `sigma50_1` | TF frame name for this detector unit |
| `update_interval_s` | `1.0` | Integration window length in seconds |
| `serial_number` | *(empty)* | HID serial number to select a specific unit; empty opens the first found |

**`d3s`**

| Parameter | Default | Description |
|---|---|---|
| `frame_id` | `d3s_1` | TF frame name for this detector unit |
| `update_interval_s` | `1.0` | Integration window length in seconds |
| `port` | *(empty)* | Serial port path; empty auto-detects `/dev/ttyACM*` |

### Running multiple units of the same type

Override `name`, `frame_id`, and `serial_number` (or `port`) per instance:

```python
Node(package="gr1", executable="gamma_detector", name="gr1_1",
     parameters=[{"frame_id": "gr1_1", "serial_number": "ABC123"}]),
Node(package="gr1", executable="gamma_detector", name="gr1_2",
     parameters=[{"frame_id": "gr1_2", "serial_number": "XYZ789"}]),
```

### Topic layout for a robot running all four detectors

```
/robot1/c12137/spectrum
/robot1/c12137/dose_rate
/robot1/c12137/temperature
/robot1/gr1/spectrum
/robot1/gr1/cps
/robot1/sigma50/spectrum
/robot1/sigma50/cps
/robot1/d3s/spectrum
/robot1/d3s/neutron_cps
/robot1/d3s/temperature
```

---

## Using RadLib as a C++ Library

The `*_comm` and `*_driver` libraries have no ROS dependency and can be integrated into any C++ application. This is the intended path for applications that want to consume detector data directly without running a ROS graph.

### Linking

After building with colcon, the installed CMake config files allow you to link against any driver:

```cmake
find_package(gr1 REQUIRED)
target_link_libraries(my_app PRIVATE gr1::gr1_driver)
```

The driver library pulls in the comm library transitively, so you only need to link the driver.

### The Driver API

All four drivers expose the same interface:

| Method | Description |
|---|---|
| `open(...)` | Opens the device. Returns `false` if the device cannot be found or opened. |
| `close()` | Closes the device and releases the handle. |
| `isOpen()` | Returns `true` if the device is currently open. |
| `read(out, interval_s)` | Blocks for `interval_s` seconds, accumulates readings, fills `out`, returns `true`. Returns `false` on a fatal device error or if the stop flag is cleared. |

The third argument to `read()` is an optional pointer to a `std::atomic<bool>`. If provided, `read()` checks it on each iteration and returns `false` as soon as it is set to `false`. This is the intended mechanism for clean shutdown from a signal handler or a controlling thread.

### Reading Structs

Each driver returns a plain C++ struct with no ROS types.

**`gr1::Reading` and `sigma50::Reading`**
```cpp
struct Reading {
    std::array<uint32_t, 4096> spectrum;  // accumulated event histogram
    double cps;                           // counts per second over the window
    double elapsed_s;                     // actual window duration
};
```

**`c12137::Reading`**
```cpp
struct Reading {
    std::array<uint32_t, 4096> spectrum;  // accumulated event histogram
    double dose_uSv_h;                    // air dose rate in µSv/h
    double temperature_c;                 // detector temperature
};
```

**`d3s::Reading`**
```cpp
struct Reading {
    std::array<uint32_t, 4096> spectrum;  // accumulated gamma histogram
    double neutron_cps;                   // neutron counts per second
    double temperature_c;                 // detector temperature
    double elapsed_s;                     // actual window duration
};
```

### Examples

**GR1 or Sigma 50:**

```cpp
#include "gr1/gr1_driver.hpp"

gr1::GR1Driver drv;
if (!drv.open()) {
    // device not found or could not be opened
    return 1;
}

gr1::Reading r;
while (drv.read(r, 1.0)) {
    // r.spectrum contains a 4096-bin histogram accumulated over ~1 second
    // r.cps is the total count rate over that window
}
```

**C12137** (provide the G(E) calibration file path explicitly outside of ROS):

```cpp
#include "c12137/c12137_driver.hpp"

c12137::C12137Driver drv;
if (!drv.open("/path/to/gef_C12137.csv", 30, 2000)) {
    return 1;
}

c12137::Reading r;
while (drv.read(r, 1.0)) {
    // r.dose_uSv_h is the computed air dose rate
    // r.spectrum is the raw histogram if you need it
}
```

**D3S:**

```cpp
#include "d3s/d3s_driver.hpp"

d3s::D3SDriver drv;
if (!drv.open()) {  // auto-detects /dev/ttyACM*
    return 1;
}

d3s::Reading r;
while (drv.read(r, 1.0)) {
    // r.neutron_cps for neutron rate
    // r.spectrum for gamma spectroscopy
}
```

**Stopping cleanly from a signal handler or another thread:**

```cpp
std::atomic<bool> running{true};

// in a signal handler or another thread:
//   running = false;

gr1::Reading r;
while (drv.read(r, 1.0, &running)) {
    // process r
}
// read() returned false because running was set to false
```

---

## Contributing

Contributions are welcome. The most useful areas right now are macOS support, Windows support, and additional detector packages.

### How to contribute

1. Fork the repository on GitHub
2. Create a branch for your change (`git checkout -b my-change`)
3. Make your changes
4. Push to your fork and open a pull request against `main`

### Adding a new detector

New detector packages should follow the same three-layer pattern as the existing ones: a `*_comm` layer for raw device I/O, a `*_driver` layer for accumulation and windowing, and an optional `*_node` layer for ROS 2 integration. The existing packages are the reference — the structure, naming conventions, and CMakeLists layout should match.

### Reporting issues

Open an issue on GitHub describing the problem, the platform you are on, and the detector you are using.

---

## Acknowledgements

- [libusb](https://libusb.info/) for cross-platform USB device access (c12137)
- [hidapi](https://github.com/libusb/hidapi) for cross-platform HID device access (gr1, sigma50)

---

## License

Copyright (C) 2026 Matt Wood (matthew.james.wood02@gmail.com)

Released under the MIT License. See [LICENSE](LICENSE) for the full text.
