# device-transport

Reusable Windows device transport library. The current module set includes
Windows serial byte I/O, shared byte encoding helpers, and XBee API-frame support.

Application concepts such as devices, database records, initialization workflows,
and telemetry storage must stay outside this repository.

## Threading Model

`serial_port` owns Windows COM-port I/O and creates the serial reader thread.

`serial_port/xbee` owns XBee API-frame building/parsing and creates a parser
thread for console/service apps without an event loop.

## Layout

```text
include/
  core/
  serial_port/
    xbee/

src/
  serial_port/
    xbee/
```

## Build

Requirements:

- CMake 3.20 or newer
- Windows
- C++11-compatible compiler

```sh
cmake -S . -B build
cmake --build build
```

Or with presets:

```sh
cmake --preset default
cmake --build --preset default
```

## CMake Usage

```cmake
add_subdirectory(path/to/device-transport)
target_link_libraries(my_app PRIVATE device_transport::desktop)
```

After install:

```cmake
find_package(device_transport CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE device_transport::desktop)
```

Available CMake targets:

- `device_transport::core` is header-only byte helpers and XBee frame protocol.
- `device_transport::serial` is Windows COM-port I/O.
- `device_transport::desktop` is XBee over `serial_port`.
- `device_transport::device_transport` is kept as a compatibility target for `device_transport::desktop`.

## C++ Usage

```cpp
#include <serial_port/xbee/xbee.hpp>

device_transport::XBee xbee;
xbee.open("/dev/ttyUSB0", 9600);
xbee.clearOutputPayload();
xbee.write8(static_cast<uint8_t>(0x01));
xbee.transmitRequest(0x0013A20000000000ULL);
```

## Validation

The desktop library target is validated with CMake/MSBuild and MinGW through the
consumer project.

## License

MIT. See `LICENSE`.
