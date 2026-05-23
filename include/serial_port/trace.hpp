#pragma once

#include <cstdint>

namespace device_transport
{
    enum class SerialTraceDirection : uint8_t
    {
        rx,
        tx
    };
}
