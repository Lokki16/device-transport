# device-transport

Reusable device transport library. The current module set includes serial byte I/O,
shared byte encoding helpers, and XBee API-frame support.

The library is intended to be embedded into different projects:

- C++ terminal/service apps through `xbee/desktop/xbee.hpp`
- Qt apps through `xbee/qt/qxbee.hpp`
- MCU firmware through `xbee/embedded/xbee.hpp`

Application concepts such as devices, database records, initialization workflows,
and telemetry storage must stay outside this repository.

## Threading Model

`xbee/protocol.hpp` and `xbee/embedded/xbee.hpp` do not create threads. They only build
frames and consume bytes supplied by the caller.

`serial_port` owns OS serial I/O.

`xbee/desktop` may create a worker thread because console/service apps do
not have an event loop by default.

`xbee/qt` does not create a worker thread. It uses `QSerialPort::readyRead`
and the Qt event loop.

## Layout

```text
include/
  core/
  serial_port/
  xbee/
    desktop/
    embedded/
    qt/

src/
  serial_port/
    windows/
    unix/
  xbee/
    desktop/
    qt/
```

## Build

Requirements:

- CMake 3.20 or newer
- C++11-compatible compiler for core, embedded, serial, and desktop targets
- Qt 5 or Qt 6 only when building `device_transport::qt`

```sh
cmake -S . -B build
cmake --build build
```

Or with presets:

```sh
cmake --preset default
cmake --build --preset default
```

Enable the Qt adapter explicitly:

```sh
cmake -S . -B build -DDEVICE_TRANSPORT_BUILD_QT_ADAPTER=ON
cmake --build build
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
- `device_transport::embedded` is header-only MCU support.
- `device_transport::serial` is desktop OS serial I/O.
- `device_transport::desktop` is desktop XBee over `serial_port`.
- `device_transport::qt` is available only with `DEVICE_TRANSPORT_BUILD_QT_ADAPTER=ON`.
- `device_transport::device_transport` is kept as a compatibility target for `device_transport::desktop`.

## C++ Usage

```cpp
#include <xbee/desktop/xbee.hpp>

device_transport::XBee xbee;
xbee.open("/dev/ttyUSB0", 9600);
xbee.clearOutputPayload();
xbee.writeToOutputPayload(static_cast<uint8_t>(0x01));
xbee.transmitRequest(0x0013A20000000000ULL);
```

## Qt Usage

Build with `DEVICE_TRANSPORT_BUILD_QT_ADAPTER=ON` and use `device_transport::QXBee`.

```cmake
target_link_libraries(my_qt_app PRIVATE device_transport::qt)
```

```cpp
#include <xbee/qt/qxbee.hpp>

device_transport::QXBee xbee;
xbee.open("/dev/ttyUSB0", 9600);
xbee.transmitRequest(0, 0x0013A20000000000ULL, 0xFFFE, QByteArray{});
```

## MCU Usage

The MCU adapter has no `std::vector`, no threads, no mutexes, and no OS serial code.
The firmware owns UART registers and interrupts; the library owns frame building,
frame parsing, payload buffers, AT constants, and byte encoding.

```cpp
#include <xbee/embedded/xbee.hpp>

device_transport::EmbeddedXBee<> xbee;
xbee.open();

xbee.pushReceivedByte(received_byte_from_uart);
uint32_t result = xbee.xbeeReceive();

xbee.clearOutputPayload();
xbee.writeToOutputPayload(static_cast<uint8_t>(0x01));
xbee.transmitRequest(0x0013A20000000000ULL);

const uint8_t *bytes = xbee.outputData();
size_t size = xbee.outputSize();
```

For MCU projects without CMake, add `include/` to the compiler include paths and include `xbee/embedded/xbee.hpp`.

## Validation

The desktop library target is validated with CMake/MSBuild and MinGW through the
consumer project. The embedded and core headers are kept C++11-compatible and do
not depend on STL containers, threads, mutexes, or operating-system serial APIs.

## License

MIT. See `LICENSE`.
