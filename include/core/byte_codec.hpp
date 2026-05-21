#pragma once

#include "core/byte_codec_core.hpp"

#include <vector>

namespace device_transport
{
    namespace byte_codec
    {
        inline uint8_t read8(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return read8(bytes.data(), offset);
        }

        inline uint16_t read16(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return read16(bytes.data(), offset);
        }

        inline uint32_t read32(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return read32(bytes.data(), offset);
        }

        inline uint64_t read64(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return read64(bytes.data(), offset);
        }

        inline void write8(std::vector<uint8_t> &buffer, const uint8_t value)
        {
            buffer.push_back(value);
        }

        inline void write16(std::vector<uint8_t> &buffer, const uint16_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value >> 8));
            buffer.push_back(static_cast<uint8_t>(value));
        }

        inline void write32(std::vector<uint8_t> &buffer, const uint32_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value >> 24));
            buffer.push_back(static_cast<uint8_t>(value >> 16));
            buffer.push_back(static_cast<uint8_t>(value >> 8));
            buffer.push_back(static_cast<uint8_t>(value));
        }

        inline void write64(std::vector<uint8_t> &buffer, const uint64_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value >> 56));
            buffer.push_back(static_cast<uint8_t>(value >> 48));
            buffer.push_back(static_cast<uint8_t>(value >> 40));
            buffer.push_back(static_cast<uint8_t>(value >> 32));
            buffer.push_back(static_cast<uint8_t>(value >> 24));
            buffer.push_back(static_cast<uint8_t>(value >> 16));
            buffer.push_back(static_cast<uint8_t>(value >> 8));
            buffer.push_back(static_cast<uint8_t>(value));
        }
    }
}
