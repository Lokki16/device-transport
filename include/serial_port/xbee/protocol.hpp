#pragma once

#include "serial_port/xbee/constants.hpp"

#include <cstddef>
#include <cstdint>

namespace device_transport
{
    namespace xbee_protocol
    {
        enum class ParseStatus : uint8_t
        {
            none,
            frameReady,
            checksumError,
            frameTooLarge
        };

        struct FrameView
        {
            const uint8_t *data{};
            size_t size{};
        };

        inline uint8_t calculateChecksum(const uint8_t *frameData, const size_t size)
        {
            uint16_t sum = 0;
            for (size_t i = 0; i < size; ++i)
            {
                sum += frameData[i];
            }
            return static_cast<uint8_t>(0xFF - (sum & 0xFF));
        }

        inline bool appendByte(uint8_t *output, const size_t capacity, size_t &outputSize, const uint8_t value)
        {
            if (outputSize + 1 > capacity)
            {
                return false;
            }

            output[outputSize++] = value;
            return true;
        }

        inline bool appendUint16(uint8_t *output, const size_t capacity, size_t &outputSize, const uint16_t value)
        {
            return appendByte(output, capacity, outputSize, static_cast<uint8_t>(value >> 8)) &&
                   appendByte(output, capacity, outputSize, static_cast<uint8_t>(value));
        }

        inline bool buildFrame(uint8_t *output, const size_t capacity, size_t &outputSize, const uint8_t *frameData, const size_t frameSize)
        {
            outputSize = 0;
            if (frameSize > 0xFFFF || (frameSize != 0 && frameData == nullptr))
            {
                return false;
            }

            if (!appendByte(output, capacity, outputSize, api_frame::startDelimiter) ||
                !appendUint16(output, capacity, outputSize, static_cast<uint16_t>(frameSize)))
            {
                return false;
            }

            for (size_t i = 0; i < frameSize; ++i)
            {
                if (!appendByte(output, capacity, outputSize, frameData[i]))
                {
                    return false;
                }
            }

            return appendByte(output, capacity, outputSize, calculateChecksum(frameData, frameSize));
        }

        template <size_t FrameCapacity>
        class FrameParser
        {
        public:
            ParseStatus process(const uint8_t byte, FrameView &frame)
            {
                frame = {};

                switch (_state)
                {
                case State::waitStart:
                    if (byte == api_frame::startDelimiter)
                    {
                        _length = 0;
                        _size = 0;
                        _state = State::readLengthMsb;
                    }
                    return ParseStatus::none;

                case State::readLengthMsb:
                    _length = static_cast<uint16_t>(byte) << 8;
                    _state = State::readLengthLsb;
                    return ParseStatus::none;

                case State::readLengthLsb:
                    _length |= byte;
                    _size = 0;
                    if (_length > FrameCapacity)
                    {
                        reset();
                        return ParseStatus::frameTooLarge;
                    }
                    _state = _length == 0 ? State::readChecksum : State::readFrameData;
                    return ParseStatus::none;

                case State::readFrameData:
                    _buffer[_size++] = byte;
                    if (_size >= _length)
                    {
                        _state = State::readChecksum;
                    }
                    return ParseStatus::none;

                case State::readChecksum:
                    if (calculateChecksum(_buffer, _length) != byte)
                    {
                        reset();
                        return ParseStatus::checksumError;
                    }

                    frame.data = _buffer;
                    frame.size = _length;
                    reset();
                    return ParseStatus::frameReady;
                }

                reset();
                return ParseStatus::none;
            }

            void reset()
            {
                _state = State::waitStart;
                _length = 0;
                _size = 0;
            }

        private:
            enum class State : uint8_t
            {
                waitStart,
                readLengthMsb,
                readLengthLsb,
                readFrameData,
                readChecksum
            };

            uint8_t _buffer[FrameCapacity]{};
            uint16_t _length{};
            size_t _size{};
            State _state{State::waitStart};
        };
    }
}
