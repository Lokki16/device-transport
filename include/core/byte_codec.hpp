#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace device_transport
{
    namespace byte_codec
    {
        inline uint8_t read8(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return bytes[offset];
        }

        inline uint16_t read16(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
        }

        inline uint32_t read32(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return (static_cast<uint32_t>(bytes[offset]) << 24) |
                   (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
                   (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
                   static_cast<uint32_t>(bytes[offset + 3]);
        }

        inline uint64_t read64(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return (static_cast<uint64_t>(bytes[offset]) << 56) |
                   (static_cast<uint64_t>(bytes[offset + 1]) << 48) |
                   (static_cast<uint64_t>(bytes[offset + 2]) << 40) |
                   (static_cast<uint64_t>(bytes[offset + 3]) << 32) |
                   (static_cast<uint64_t>(bytes[offset + 4]) << 24) |
                   (static_cast<uint64_t>(bytes[offset + 5]) << 16) |
                   (static_cast<uint64_t>(bytes[offset + 6]) << 8) |
                   static_cast<uint64_t>(bytes[offset + 7]);
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
