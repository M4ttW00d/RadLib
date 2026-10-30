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
- [Device Permissions](#device-permissions)
- [Platform Support](#platform-support)
- [Building with ROS 2](#building-with-ros-2)
- [Usage with ROS 2](#usage-with-ros-2)
- [Docker](#docker)
- [Using RadLib as a C++ Library](#using-radlib-as-a-c-library)
- [Contributing](#contributing)
- [Acknowledgements](#acknowledgements)
- [License](#license)

---

## Supported Hardware

### Hamamatsu C12137

A compact gamma-ray detection module combining a scintillator crystal with an MPPC (Multi-Pixel Photon Counter) silicon photodetector. Onboard signal processing and A/D conversion produce a 4096-channel energy spectrum over USB. RadLib computes air dose rate in µSv/h from the spectrum using a G(E) conversion factor table bundled with the package.

Two tables are bundled in `c12137/data`: `gef_C12137.csv` for the C12137 and C12137-00D, and `gef_C12137-01.csv` for the larger C12137-01. Both the ROS node and the examples default to the first, so a -01 unit must be pointed at the second through `ge_function_file` (or the first argument of `open()`), otherwise its dose rate reads roughly twelve times too high. The dose rate is the sum of counts times G(E) over the energy window, treating the table values as pSv per count.

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

A wearable personal radiation detector combining a CsI(Tl) gamma spectrometer (30 keV to 3 MeV) with a compact thermal neutron scintillator for simultaneous dual-particle detection. Designed for first responders and networked radiation monitoring. Connects over USB or wirelessly via Bluetooth SPP; either way it appears as a serial port on the host system.

- Connection: 115200 baud serial over USB (CDC-ACM, `/dev/ttyACM*`) or Bluetooth SPP (`/dev/rfcomm*`, see [D3S over Bluetooth](#d3s-over-bluetooth))
- Outputs: gamma spectrum, neutron counts per second, detector temperature
- Auto-detects by probing every `/dev/ttyACM*` then every `/dev/rfcomm*` and keeping the first unit whose serial number starts with `SGM`

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

## Device Permissions

On Linux the detectors are only accessible as root until udev rules grant access. Install the bundled rules once:

```bash
sudo cp udev/99-radlib.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then reconnect the detectors. The GR1 and Sigma 50 are read through `/dev/hidraw*`, the C12137 through `/dev/bus/usb`, and the D3S through `/dev/ttyACM*` over USB (membership of the `dialout` group also covers that one).

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
| `ge_function_file` | *(bundled)* | Path to G(E) CSV; empty uses the bundled `gef_C12137.csv` (set it to `gef_C12137-01.csv` for a C12137-01) |
| `energy_lower_kev` | `30` | Lower bound of dose rate integration window (keV) |
| `energy_upper_kev` | `2000` | Upper bound of dose rate integration window (keV) |

**`gr1` and `sigma50`**

| Parameter | Default | Description |
|---|---|---|
| `frame_id` | `gr1_1` / `sigma50_1` | TF frame name for this detector unit (reserved; no GR1 or Sigma 50 message carries a header yet) |
| `update_interval_s` | `1.0` | Integration window length in seconds |
| `serial_number` | *(empty)* | USB serial string to select a specific unit; empty opens the first found. A unit that reports no serial string can only be opened with an empty value |

**`d3s`**

| Parameter | Default | Description |
|---|---|---|
| `frame_id` | `d3s_1` | TF frame name for this detector unit |
| `update_interval_s` | `1.0` | Integration window length in seconds |
| `port` | *(empty)* | Serial port path; empty probes `/dev/ttyACM*` then `/dev/rfcomm*` |

### Running multiple units of the same type

Override `name`, `frame_id`, and `serial_number` (or `port`) per instance. Every instance needs a distinct `serial_number` or `port`: two nodes with the value left empty both open the first unit found and read the same detector twice.

```python
Node(package="gr1", executable="gamma_detector", name="gr1_1",
     parameters=[{"frame_id": "gr1_1", "serial_number": "ABC123"}]),
Node(package="gr1", executable="gamma_detector", name="gr1_2",
     parameters=[{"frame_id": "gr1_2", "serial_number": "XYZ789"}]),
```

### Topic layout for a robot running all four detectors

The `spectrum` topics are `std_msgs/UInt16MultiArray` messages without a header, so they carry no timestamp or frame. Each channel count is clamped to 65535 per window, which only matters for long windows on strong sources. `cps`, `neutron_cps` and `dose_rate` are `std_msgs/Float64`; `temperature` is `sensor_msgs/Temperature` and does carry the stamp and `frame_id`.

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

## Docker

A prebuilt image with all four nodes is published at `ghcr.io/m4ttw00d/radlib`. It starts the detectors named in `DETECTORS` under the `ROBOT_NAME` namespace and records every topic to a bag under `/data`:

```bash
docker run --network=host \
  -v /host/data:/data \
  --device /dev/hidraw0 \
  --device /dev/ttyACM0 \
  -e ROBOT_NAME=robot1 \
  -e DETECTORS=gr1,d3s \
  ghcr.io/m4ttw00d/radlib:latest
```

Pass the device nodes the chosen detectors need, or run with `--privileged`:

| Detector | Device flag |
|---|---|
| `gr1`, `sigma50` | `--device /dev/hidrawN`, the kernel HID node rather than `/dev/bus/usb`; find N with `ls -l /dev/hidraw*` |
| `c12137` | `--device /dev/bus/usb` |
| `d3s` | `--device /dev/ttyACM0` over USB, or `/dev/rfcomm0` over Bluetooth |

The container exits with a non-zero status as soon as any node or the recorder stops, so a `--restart` policy can bring it back. Run it without `DETECTORS` to print the usage text.

---

## Using RadLib as a C++ Library

The comm and driver layers have no ROS dependency and can be built and used without a ROS installation. A root `CMakeLists.txt` is provided for this purpose.

### Building

Install the system dependencies, then build and install with plain CMake:

```bash
sudo apt-get install -y libusb-1.0-0-dev libhidapi-dev
git clone https://github.com/M4ttW00d/RadLib.git
cd RadLib
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

Add `-DRADLIB_BUILD_TESTS=ON` to the `cmake` line to build the unit tests as well (they need `libgtest-dev`), then run `ctest` in the build directory. A complete consumer project that links every driver is in `examples/minimal`.

### Linking from your own project

After installing, add RadLib to your CMake project:

```cmake
find_package(radlib REQUIRED)
target_link_libraries(my_app PRIVATE radlib::gr1_driver)
```

Each driver pulls in its comm library transitively, so you only need to link the driver. Available targets: `radlib::gr1_driver`, `radlib::sigma50_driver`, `radlib::c12137_driver`, `radlib::d3s_driver`.

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
    std::array<uint32_t, 4096> spectrum;  // every event, regardless of the energy window
    double dose_uSv_h;                    // air dose rate in µSv/h from events inside the window
    double temperature_c;                 // detector temperature
    double elapsed_s;                     // detector time integrated (whole 100 ms packets)
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

Each example below is a complete program. Signal handling is included so Ctrl+C stops the read loop cleanly without interrupting mid-window.

**GR1 or Sigma 50**

```cpp
#include <atomic>
#include <csignal>
#include <cstdio>

#include "gr1/gr1_driver.hpp"

static std::atomic<bool> running{true};
static void onSignal(int) { running = false; }

int main()
{
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    gr1::GR1Driver drv;
    if (!drv.open()) {
        std::fprintf(stderr, "error: could not open GR1 (is it plugged in?)\n");
        return 1;
    }

    gr1::Reading r;
    while (drv.read(r, 1.0, &running)) {
        std::printf("cps: %.1f  elapsed: %.2f s\n", r.cps, r.elapsed_s);
        // r.spectrum[i] is the count in channel i over the last window
    }

    drv.close();
    return 0;
}
```

For the Sigma 50, replace `gr1::GR1Driver` with `sigma50::Sigma50Driver` and include `sigma50/sigma50_driver.hpp`. The rest is identical.

**C12137**

The G(E) calibration file is at `c12137/data/gef_C12137.csv` in the repository, and installs to `share/radlib/c12137/gef_C12137.csv`. Pass its path to `open()`.

```cpp
#include <atomic>
#include <csignal>
#include <cstdio>

#include "c12137/c12137_driver.hpp"

static std::atomic<bool> running{true};
static void onSignal(int) { running = false; }

int main()
{
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    c12137::C12137Driver drv;
    if (!drv.open("/path/to/gef_C12137.csv")) {
        std::fprintf(stderr, "error: could not open C12137\n");
        return 1;
    }

    c12137::Reading r;
    while (drv.read(r, 1.0, &running)) {
        std::printf("dose: %.4f uSv/h  temp: %.1f C\n", r.dose_uSv_h, r.temperature_c);
        // r.spectrum[i] is the raw event count in channel i
    }

    drv.close();
    return 0;
}
```

The second and third arguments to `open()` set the energy integration window in keV (defaults: 30 and 2000). Events outside this window are excluded from the dose calculation but still appear in the spectrum. The interval passed to `read()` is rounded to whole 100 ms device packets, and `elapsed_s` reports the time actually integrated.

**D3S**

```cpp
#include <atomic>
#include <csignal>
#include <cstdio>

#include "d3s/d3s_driver.hpp"

static std::atomic<bool> running{true};
static void onSignal(int) { running = false; }

int main()
{
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    d3s::D3SDriver drv;
    if (!drv.open()) {  // auto-detects the first /dev/ttyACM* that responds as a D3S
        std::fprintf(stderr, "error: could not open D3S (is Bluetooth paired?)\n");
        return 1;
    }

    d3s::Reading r;
    while (drv.read(r, 1.0, &running)) {
        std::printf("neutron cps: %.2f  temp: %.0f C\n", r.neutron_cps, r.temperature_c);
        // r.spectrum[i] is the gamma count in channel i
    }

    drv.close();
    return 0;
}
```

To connect to a specific port rather than auto-detecting, pass the path to `open()`:

```cpp
drv.open("/dev/ttyACM1");
```

The first spectrum the D3S returns after a pause covers everything it accumulated since it was last polled, so the driver requests and discards one frame after `open()`.

### D3S over Bluetooth

Pair the unit once, then bind an RFCOMM device to its serial port profile channel. The device node exists immediately and the link comes up when the port is opened:

```bash
bluetoothctl pair AA:BB:CC:DD:EE:FF
sudo rfcomm bind 0 AA:BB:CC:DD:EE:FF 1    # creates /dev/rfcomm0
```

Auto-detection probes `/dev/rfcomm*` after `/dev/ttyACM*`. To skip probing, pass the path explicitly through the `port` parameter or `drv.open("/dev/rfcomm0")`.

---

## Contributing

Contributions are welcome. The most useful areas are additional detector packages.

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
