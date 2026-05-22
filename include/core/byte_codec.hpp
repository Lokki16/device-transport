#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace device_transport
{
    namespace byte_codec
    {
        inline uint8_t read8(const uint8_t *bytes, const size_t offset = 0)
        {
            return bytes[offset];
        }

        inline uint16_t read16(const uint8_t *bytes, const size_t offset = 0)
        {
            return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
        }

        inline uint32_t read32(const uint8_t *bytes, const size_t offset = 0)
        {
            return (static_cast<uint32_t>(bytes[offset]) << 24) |
                   (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
                   (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
                   static_cast<uint32_t>(bytes[offset + 3]);
        }

        inline uint64_t read64(const uint8_t *bytes, const size_t offset = 0)
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

        inline bool write8(uint8_t *buffer, const size_t capacity, size_t &offset, const uint8_t value)
        {
            if (offset + 1 > capacity)
            {
                return false;
            }

            buffer[offset++] = value;
            return true;
        }

        inline bool write16(uint8_t *buffer, const size_t capacity, size_t &offset, const uint16_t value)
        {
            return write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 8)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value));
        }

        inline bool write32(uint8_t *buffer, const size_t capacity, size_t &offset, const uint32_t value)
        {
            return write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 24)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 16)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 8)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value));
        }

        inline bool write64(uint8_t *buffer, const size_t capacity, size_t &offset, const uint64_t value)
        {
            return write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 56)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 48)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 40)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 32)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 24)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 16)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value >> 8)) &&
                   write8(buffer, capacity, offset, static_cast<uint8_t>(value));
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

        inline uint8_t sumOfBytes(const uint16_t value)
        {
            return static_cast<uint8_t>((value >> 8) + value);
        }

        inline uint8_t sumOfBytes(const uint32_t value)
        {
            return static_cast<uint8_t>((value >> 24) + (value >> 16) + (value >> 8) + value);
        }

        inline uint8_t sumOfBytes(const uint64_t value)
        {
            return static_cast<uint8_t>((value >> 56) + (value >> 48) + (value >> 40) + (value >> 32) +
                                        (value >> 24) + (value >> 16) + (value >> 8) + value);
        }

        inline uint32_t combineBytes(const uint8_t byte1, const uint8_t byte2, const uint8_t byte3, const uint8_t byte4)
        {
            return (static_cast<uint32_t>(byte1) << 24) |
                   (static_cast<uint32_t>(byte2) << 16) |
                   (static_cast<uint32_t>(byte3) << 8) |
                   static_cast<uint32_t>(byte4);
        }
    }
}
