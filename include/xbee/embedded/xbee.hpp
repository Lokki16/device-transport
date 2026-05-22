#pragma once

#include "core/byte_codec.hpp"
#include "xbee/constants.hpp"
#include "xbee/parameters.hpp"
#include "xbee/protocol.hpp"

#include <cstddef>
#include <cstdint>

namespace device_transport
{
    template <size_t InputCapacity = 256, size_t OutputCapacity = 256>
    class EmbeddedSerialBuffer
    {
    public:
        uint16_t bytesToRead() const
        {
            return static_cast<uint16_t>((_inputWrite - _inputRead) & _inputMask);
        }

        uint16_t bytesToWrite() const { return static_cast<uint16_t>(_outputSize); }

        uint8_t read8()
        {
            if (bytesToRead() < 1)
            {
                return 0xFF;
            }

            const uint8_t value = _input[_inputRead];
            _inputRead = (_inputRead + 1) & _inputMask;
            return value;
        }

        uint16_t read16()
        {
            if (bytesToRead() < 2)
            {
                return 0xFFFF;
            }

            uint16_t value = 0;
            value |= static_cast<uint16_t>(read8()) << 8;
            value |= read8();
            return value;
        }

        uint32_t read32()
        {
            if (bytesToRead() < 4)
            {
                return 0xFFFFFFFF;
            }

            uint32_t value = 0;
            for (uint8_t i = 0; i < 4; ++i)
            {
                value = (value << 8) | read8();
            }
            return value;
        }

        uint64_t read64()
        {
            if (bytesToRead() < 8)
            {
                return 0xFFFFFFFFFFFFFFFF;
            }

            uint64_t value = 0;
            for (uint8_t i = 0; i < 8; ++i)
            {
                value = (value << 8) | read8();
            }
            return value;
        }

        bool pushReceivedByte(const uint8_t byte)
        {
            const size_t next = (_inputWrite + 1) & _inputMask;
            if (next == _inputRead)
            {
                return false;
            }

            _input[_inputWrite] = byte;
            _inputWrite = next;
            return true;
        }

        uint32_t write8(const uint8_t value)
        {
            return byte_codec::write8(_output, OutputCapacity, _outputSize, value) ? 1
                                                                                   : 0;
        }

        uint32_t write16(const uint16_t value)
        {
            return byte_codec::write16(_output, OutputCapacity, _outputSize, value) ? 2
                                                                                    : 0;
        }

        uint32_t write32(const uint32_t value)
        {
            return byte_codec::write32(_output, OutputCapacity, _outputSize, value) ? 4
                                                                                    : 0;
        }

        uint32_t write64(const uint64_t value)
        {
            return byte_codec::write64(_output, OutputCapacity, _outputSize, value) ? 8
                                                                                    : 0;
        }

        const uint8_t *outputData() const { return _output; }

        size_t outputSize() const { return _outputSize; }

        void clearInputBuffer()
        {
            _inputRead = 0;
            _inputWrite = 0;
        }

        void clearOutputBuffer() { _outputSize = 0; }

    private:
        static_assert(InputCapacity > 1, "InputCapacity must be greater than 1");
        static_assert((InputCapacity & (InputCapacity - 1)) == 0,
                      "InputCapacity must be a power of two");

        static constexpr size_t _inputMask = InputCapacity - 1;

        uint8_t _input[InputCapacity]{};
        uint8_t _output[OutputCapacity]{};
        size_t _inputRead{};
        size_t _inputWrite{};
        size_t _outputSize{};
    };

    class FrameId
    {
    public:
        uint8_t keep(const uint8_t frameId)
        {
            if (frameId == 0)
            {
                return 0xFF;
            }

            const uint8_t index = frameId / 64;
            const uint8_t bit = frameId % 64;
            const uint64_t mask = uint64_t{1} << bit;
            if ((_flags[index] & mask) != 0)
            {
                return 0xFE;
            }

            _flags[index] |= mask;
            return 0x00;
        }

        uint8_t free(const uint8_t frameId)
        {
            if (frameId == 0)
            {
                return 0xFF;
            }

            const uint8_t index = frameId / 64;
            const uint8_t bit = frameId % 64;
            const uint64_t mask = uint64_t{1} << bit;
            if ((_flags[index] & mask) == 0)
            {
                return 0xFF;
            }

            _flags[index] &= ~mask;
            return 0x00;
        }

        uint8_t keepNext()
        {
            for (uint16_t id = 1; id <= 255; ++id)
            {
                const uint8_t frameId = static_cast<uint8_t>(id);
                if (isFree(frameId) == 0)
                {
                    keep(frameId);
                    return frameId;
                }
            }

            return 0xFF;
        }

        uint8_t isFree(const uint8_t frameId) const
        {
            if (frameId == 0)
            {
                return 0xFF;
            }

            const uint8_t index = frameId / 64;
            const uint8_t bit = frameId % 64;
            return (_flags[index] & (uint64_t{1} << bit)) == 0 ? 0x00 : 0x01;
        }

        void clear()
        {
            for (uint8_t i = 0; i < 4; ++i)
            {
                _flags[i] = 0;
            }
        }

    private:
        uint64_t _flags[4]{};
    };

    template <size_t InputCapacity = 256, size_t OutputCapacity = 256,
              size_t PayloadCapacity = 84, size_t ValueCapacity = 16,
              size_t FrameCapacity = 128>
    class EmbeddedXBee
    {
    public:
        EmbeddedSerialBuffer<InputCapacity, OutputCapacity> serial;
        FrameId frameIds;
        AtParameters parameters;

        uint64_t xbee64Id{};
        uint16_t xbee16Id{};
        uint8_t receiveOptions{};
        uint8_t frameId{};
        uint16_t atCommand{};
        uint8_t status{};
        uint8_t discoveryStatus{};

        uint8_t open()
        {
            clearInputPayload();
            clearOutputPayload();
            clearParsedValue();
            serial.clearInputBuffer();
            serial.clearOutputBuffer();
            frameIds.clear();
            _resetParser();
            _open = true;
            return 0;
        }

        void close()
        {
            serial.clearInputBuffer();
            serial.clearOutputBuffer();
            _resetParser();
            _open = false;
        }

        bool isOpen() const { return _open; }

        bool pushReceivedByte(const uint8_t byte)
        {
            return serial.pushReceivedByte(byte);
        }

        uint16_t bytesToRead() const { return serial.bytesToRead(); }

        uint16_t bytesToWrite() const { return serial.bytesToWrite(); }

        uint8_t read8() { return serial.read8(); }

        uint16_t read16() { return serial.read16(); }

        uint32_t read32() { return serial.read32(); }

        uint64_t read64() { return serial.read64(); }

        uint32_t write8(const uint8_t value) { return serial.write8(value); }

        uint32_t write16(const uint16_t value) { return serial.write16(value); }

        uint32_t write32(const uint32_t value) { return serial.write32(value); }

        uint32_t write64(const uint64_t value) { return serial.write64(value); }

        const uint8_t *outputData() const { return serial.outputData(); }

        size_t outputSize() const { return serial.outputSize(); }

        void clearInputBuffer() { serial.clearInputBuffer(); }

        void clearOutputBuffer() { serial.clearOutputBuffer(); }

        void clearOutputPayload() { _outputPayloadSize = 0; }

        bool writeToOutputPayload(const uint8_t value)
        {
            return byte_codec::write8(_outputPayload, PayloadCapacity,
                                      _outputPayloadSize, value);
        }

        bool writeToOutputPayload(const uint16_t value)
        {
            return byte_codec::write16(_outputPayload, PayloadCapacity,
                                       _outputPayloadSize, value);
        }

        bool writeToOutputPayload(const uint32_t value)
        {
            return byte_codec::write32(_outputPayload, PayloadCapacity,
                                       _outputPayloadSize, value);
        }

        bool writeToOutputPayload(const uint64_t value)
        {
            return byte_codec::write64(_outputPayload, PayloadCapacity,
                                       _outputPayloadSize, value);
        }

        const uint8_t *inputPayloadData() const { return _inputPayload; }

        size_t inputPayloadSize() const { return _inputPayloadSize; }

        const uint8_t *outputPayloadData() const { return _outputPayload; }

        size_t outputPayloadSize() const { return _outputPayloadSize; }

        const uint8_t *valueData() const { return _value; }

        size_t valueSize() const { return _valueSize; }

        void clearInputPayload()
        {
            _inputPayloadSize = 0;
            _inputPayloadReadIndex = 0;
        }

        uint8_t readFromInputPayload8()
        {
            if (_inputPayloadReadIndex >= _inputPayloadSize)
            {
                return 0xFF;
            }

            return _inputPayload[_inputPayloadReadIndex++];
        }

        uint16_t readFromInputPayload16()
        {
            uint16_t value = 0;
            value |= static_cast<uint16_t>(readFromInputPayload8()) << 8;
            value |= readFromInputPayload8();
            return value;
        }

        uint32_t readFromInputPayload32()
        {
            uint32_t value = 0;
            for (uint8_t i = 0; i < 4; ++i)
            {
                value = (value << 8) | readFromInputPayload8();
            }
            return value;
        }

        uint64_t readFromInputPayload64()
        {
            uint64_t value = 0;
            for (uint8_t i = 0; i < 8; ++i)
            {
                value = (value << 8) | readFromInputPayload8();
            }
            return value;
        }

        uint8_t atCommandRequest(const uint16_t command)
        {
            return _atCommandRequest(true, command, nullptr, 0);
        }

        uint8_t atCommandRequest(const uint16_t command, const uint8_t value)
        {
            uint8_t bytes[1]{value};
            return _atCommandRequest(true, command, bytes, sizeof(bytes));
        }

        uint8_t atCommandRequest(const uint16_t command, const uint16_t value)
        {
            uint8_t bytes[2]{};
            size_t offset = 0;
            byte_codec::write16(bytes, sizeof(bytes), offset, value);
            return _atCommandRequest(true, command, bytes, sizeof(bytes));
        }

        uint8_t atCommandRequest(const uint16_t command, const uint32_t value)
        {
            uint8_t bytes[4]{};
            size_t offset = 0;
            byte_codec::write32(bytes, sizeof(bytes), offset, value);
            return _atCommandRequest(true, command, bytes, sizeof(bytes));
        }

        uint8_t atCommandRequest(const uint16_t command, const uint64_t value)
        {
            uint8_t bytes[8]{};
            size_t offset = 0;
            byte_codec::write64(bytes, sizeof(bytes), offset, value);
            return _atCommandRequest(true, command, bytes, sizeof(bytes));
        }

        uint8_t atCommandRequest(const bool assignId, const uint16_t command)
        {
            return _atCommandRequest(assignId, command, nullptr, 0);
        }

        uint8_t atCommandRequest(const bool assignId, const uint16_t command,
                                 const uint8_t value)
        {
            uint8_t bytes[1]{value};
            return _atCommandRequest(assignId, command, bytes, sizeof(bytes));
        }

        uint8_t atCommandRequest(const bool assignId, const uint16_t command,
                                 const uint16_t value)
        {
            uint8_t bytes[2]{};
            size_t offset = 0;
            byte_codec::write16(bytes, sizeof(bytes), offset, value);
            return _atCommandRequest(assignId, command, bytes, sizeof(bytes));
        }

        uint8_t atCommandRequest(const bool assignId, const uint16_t command,
                                 const uint32_t value)
        {
            uint8_t bytes[4]{};
            size_t offset = 0;
            byte_codec::write32(bytes, sizeof(bytes), offset, value);
            return _atCommandRequest(assignId, command, bytes, sizeof(bytes));
        }

        uint8_t atCommandRequest(const bool assignId, const uint16_t command,
                                 const uint64_t value)
        {
            uint8_t bytes[8]{};
            size_t offset = 0;
            byte_codec::write64(bytes, sizeof(bytes), offset, value);
            return _atCommandRequest(assignId, command, bytes, sizeof(bytes));
        }

        uint8_t remoteAtCommandRequest(const uint64_t destinationSn,
                                       const uint16_t command,
                                       const uint16_t destinationNa = 0xFFFE,
                                       const uint8_t rco = 0x02)
        {
            return _remoteAtCommandRequest(true, destinationSn, destinationNa, rco,
                                           command, nullptr, 0);
        }

        uint8_t remoteAtCommandRequest(const uint64_t destinationSn,
                                       const uint16_t command, const uint8_t value,
                                       const uint16_t destinationNa = 0xFFFE,
                                       const uint8_t rco = 0x02)
        {
            uint8_t bytes[1]{value};
            return _remoteAtCommandRequest(true, destinationSn, destinationNa, rco,
                                           command, bytes, sizeof(bytes));
        }

        uint8_t remoteAtCommandRequest(const uint64_t destinationSn,
                                       const uint16_t command, const uint16_t value,
                                       const uint16_t destinationNa = 0xFFFE,
                                       const uint8_t rco = 0x02)
        {
            uint8_t bytes[2]{};
            size_t offset = 0;
            byte_codec::write16(bytes, sizeof(bytes), offset, value);
            return _remoteAtCommandRequest(true, destinationSn, destinationNa, rco,
                                           command, bytes, sizeof(bytes));
        }

        uint8_t remoteAtCommandRequest(const uint64_t destinationSn,
                                       const uint16_t command, const uint32_t value,
                                       const uint16_t destinationNa = 0xFFFE,
                                       const uint8_t rco = 0x02)
        {
            uint8_t bytes[4]{};
            size_t offset = 0;
            byte_codec::write32(bytes, sizeof(bytes), offset, value);
            return _remoteAtCommandRequest(true, destinationSn, destinationNa, rco,
                                           command, bytes, sizeof(bytes));
        }

        uint8_t remoteAtCommandRequest(const uint64_t destinationSn,
                                       const uint16_t command, const uint64_t value,
                                       const uint16_t destinationNa = 0xFFFE,
                                       const uint8_t rco = 0x02)
        {
            uint8_t bytes[8]{};
            size_t offset = 0;
            byte_codec::write64(bytes, sizeof(bytes), offset, value);
            return _remoteAtCommandRequest(true, destinationSn, destinationNa, rco,
                                           command, bytes, sizeof(bytes));
        }

        uint8_t remoteAtCommandRequest(const bool assignId,
                                       const uint64_t destinationSn,
                                       const uint16_t destinationNa,
                                       const uint8_t rco, const uint16_t command)
        {
            return _remoteAtCommandRequest(assignId, destinationSn, destinationNa, rco,
                                           command, nullptr, 0);
        }

        uint8_t remoteAtCommandRequest(const bool assignId,
                                       const uint64_t destinationSn,
                                       const uint16_t destinationNa,
                                       const uint8_t rco, const uint16_t command,
                                       const uint8_t value)
        {
            uint8_t bytes[1]{value};
            return _remoteAtCommandRequest(assignId, destinationSn, destinationNa, rco,
                                           command, bytes, sizeof(bytes));
        }

        uint8_t remoteAtCommandRequest(const bool assignId,
                                       const uint64_t destinationSn,
                                       const uint16_t destinationNa,
                                       const uint8_t rco, const uint16_t command,
                                       const uint16_t value)
        {
            uint8_t bytes[2]{};
            size_t offset = 0;
            byte_codec::write16(bytes, sizeof(bytes), offset, value);
            return _remoteAtCommandRequest(assignId, destinationSn, destinationNa, rco,
                                           command, bytes, sizeof(bytes));
        }

        uint8_t remoteAtCommandRequest(const bool assignId,
                                       const uint64_t destinationSn,
                                       const uint16_t destinationNa,
                                       const uint8_t rco, const uint16_t command,
                                       const uint32_t value)
        {
            uint8_t bytes[4]{};
            size_t offset = 0;
            byte_codec::write32(bytes, sizeof(bytes), offset, value);
            return _remoteAtCommandRequest(assignId, destinationSn, destinationNa, rco,
                                           command, bytes, sizeof(bytes));
        }

        uint8_t remoteAtCommandRequest(const bool assignId,
                                       const uint64_t destinationSn,
                                       const uint16_t destinationNa,
                                       const uint8_t rco, const uint16_t command,
                                       const uint64_t value)
        {
            uint8_t bytes[8]{};
            size_t offset = 0;
            byte_codec::write64(bytes, sizeof(bytes), offset, value);
            return _remoteAtCommandRequest(assignId, destinationSn, destinationNa, rco,
                                           command, bytes, sizeof(bytes));
        }

        uint8_t transmitRequest(const uint64_t destinationSn,
                                const uint16_t destinationNa = 0xFFFE,
                                const uint8_t broadcastRadius = 0x00,
                                const uint8_t options = 0x00)
        {
            return _transmitRequest(true, destinationSn, destinationNa, broadcastRadius,
                                    options, _outputPayload, _outputPayloadSize);
        }

        uint8_t transmitRequest(const bool assignId, const uint64_t destinationSn,
                                const uint16_t destinationNa = 0xFFFE,
                                const uint8_t broadcastRadius = 0x00,
                                const uint8_t options = 0x00)
        {
            return _transmitRequest(assignId, destinationSn, destinationNa,
                                    broadcastRadius, options, _outputPayload,
                                    _outputPayloadSize);
        }

        uint8_t transmitRequest(const uint64_t destinationSn, const uint8_t *payload,
                                const size_t payloadSize,
                                const uint16_t destinationNa = 0xFFFE,
                                const uint8_t broadcastRadius = 0x00,
                                const uint8_t options = 0x00)
        {
            return _transmitRequest(true, destinationSn, destinationNa, broadcastRadius,
                                    options, payload, payloadSize);
        }

        uint8_t transmitRequest(const bool assignId, const uint64_t destinationSn,
                                const uint8_t *payload, const size_t payloadSize,
                                const uint16_t destinationNa = 0xFFFE,
                                const uint8_t broadcastRadius = 0x00,
                                const uint8_t options = 0x00)
        {
            return _transmitRequest(assignId, destinationSn, destinationNa,
                                    broadcastRadius, options, payload, payloadSize);
        }

        uint32_t xbeeReceive()
        {
            while (serial.bytesToRead() > 0)
            {
                const uint32_t result = processByte(serial.read8());
                if (result != 0)
                {
                    return result;
                }
            }

            return 0;
        }

        uint32_t processByte(const uint8_t byte)
        {
            xbee_protocol::FrameView frame;
            const xbee_protocol::ParseStatus status = _parser.process(byte, frame);
            if (status == xbee_protocol::ParseStatus::none)
            {
                return 0;
            }

            if (status == xbee_protocol::ParseStatus::frameTooLarge)
            {
                return 0xFFFFFFFC;
            }

            if (status == xbee_protocol::ParseStatus::checksumError)
            {
                return 0xFFFFFFFF;
            }

            return _parseFrame(frame.data, frame.size);
        }

        static uint8_t calculateChecksum(const uint8_t *frameData,
                                         const size_t size)
        {
            return xbee_protocol::calculateChecksum(frameData, size);
        }

    private:
        uint8_t _inputPayload[PayloadCapacity]{};
        uint8_t _outputPayload[PayloadCapacity]{};
        uint8_t _value[ValueCapacity]{};
        uint8_t _frameData[FrameCapacity]{};
        xbee_protocol::FrameParser<FrameCapacity> _parser;
        size_t _inputPayloadSize{};
        size_t _outputPayloadSize{};
        size_t _inputPayloadReadIndex{};
        size_t _valueSize{};
        bool _open{};

        void clearParsedValue() { _valueSize = 0; }

        void _resetParser() { _parser.reset(); }

        uint8_t _nextFrameIdForRequest(const bool assignId)
        {
            return assignId ? frameIds.keepNext() : 0;
        }

        bool _writeFrame(const uint8_t *frameData, const size_t frameSize)
        {
            uint8_t output[OutputCapacity]{};
            size_t outputSize = 0;
            if (!xbee_protocol::buildFrame(output, OutputCapacity, outputSize,
                                           frameData, frameSize))
            {
                return false;
            }

            serial.clearOutputBuffer();
            for (size_t i = 0; i < outputSize; ++i)
            {
                if (serial.write8(output[i]) != 1)
                {
                    return false;
                }
            }
            return true;
        }

        uint8_t _atCommandRequest(const bool assignId, const uint16_t command,
                                  const uint8_t *parameter,
                                  const size_t parameterSize)
        {
            uint8_t frame[13]{};
            size_t offset = 0;
            const uint8_t requestFrameId = _nextFrameIdForRequest(assignId);
            byte_codec::write8(frame, sizeof(frame), offset,
                               api_frame::type::atCommandRequest);
            byte_codec::write8(frame, sizeof(frame), offset, requestFrameId);
            byte_codec::write16(frame, sizeof(frame), offset, command);
            for (size_t i = 0; i < parameterSize; ++i)
            {
                byte_codec::write8(frame, sizeof(frame), offset, parameter[i]);
            }

            return _writeFrame(frame, offset) ? requestFrameId : 0xFF;
        }

        uint8_t _remoteAtCommandRequest(const bool assignId,
                                        const uint64_t destinationSn,
                                        const uint16_t destinationNa,
                                        const uint8_t rco, const uint16_t command,
                                        const uint8_t *parameter,
                                        const size_t parameterSize)
        {
            uint8_t frame[23]{};
            size_t offset = 0;
            const uint8_t requestFrameId = _nextFrameIdForRequest(assignId);
            byte_codec::write8(frame, sizeof(frame), offset,
                               api_frame::type::remoteAtCommandRequest);
            byte_codec::write8(frame, sizeof(frame), offset, requestFrameId);
            byte_codec::write64(frame, sizeof(frame), offset, destinationSn);
            byte_codec::write16(frame, sizeof(frame), offset, destinationNa);
            byte_codec::write8(frame, sizeof(frame), offset, rco);
            byte_codec::write16(frame, sizeof(frame), offset, command);
            for (size_t i = 0; i < parameterSize; ++i)
            {
                byte_codec::write8(frame, sizeof(frame), offset, parameter[i]);
            }

            return _writeFrame(frame, offset) ? requestFrameId : 0xFF;
        }

        uint8_t _transmitRequest(const bool assignId, const uint64_t destinationSn,
                                 const uint16_t destinationNa,
                                 const uint8_t broadcastRadius, const uint8_t options,
                                 const uint8_t *payload, const size_t payloadSize)
        {
            if (payloadSize + 14 > FrameCapacity)
            {
                return 0xFF;
            }

            size_t offset = 0;
            const uint8_t requestFrameId = _nextFrameIdForRequest(assignId);
            byte_codec::write8(_frameData, FrameCapacity, offset,
                               api_frame::type::transmitRequest);
            byte_codec::write8(_frameData, FrameCapacity, offset, requestFrameId);
            byte_codec::write64(_frameData, FrameCapacity, offset, destinationSn);
            byte_codec::write16(_frameData, FrameCapacity, offset, destinationNa);
            byte_codec::write8(_frameData, FrameCapacity, offset, broadcastRadius);
            byte_codec::write8(_frameData, FrameCapacity, offset, options);
            for (size_t i = 0; i < payloadSize; ++i)
            {
                byte_codec::write8(_frameData, FrameCapacity, offset, payload[i]);
            }

            if (_writeFrame(_frameData, offset))
            {
                _outputPayloadSize = 0;
                return requestFrameId;
            }

            return 0xFF;
        }

        uint32_t _parseFrame(const uint8_t *frameData, const size_t frameLength)
        {
            if (frameLength == 0)
            {
                return 0xFFFFFFFE;
            }

            const uint8_t frameType = frameData[0];
            switch (frameType)
            {
            case api_frame::type::receivePacket:
                return _parseReceivePacket(frameData, frameLength);
            case api_frame::type::atCommandResponse:
                return _parseAtCommandResponse(frameData, frameLength);
            case api_frame::type::modemStatus:
                return _parseModemStatus(frameData, frameLength);
            case api_frame::type::transmitStatus:
                return _parseTransmitStatus(frameData, frameLength);
            case api_frame::type::remoteAtCommandResponse:
                return _parseRemoteAtCommandResponse(frameData, frameLength);
            default:
                return 0xFFFFFFFE;
            }
        }

        uint32_t _parseReceivePacket(const uint8_t *frameData,
                                     const size_t frameLength)
        {
            if (frameLength < 12)
            {
                return 0xFFFFFFFC;
            }

            xbee64Id = byte_codec::read64(frameData, 1);
            xbee16Id = byte_codec::read16(frameData, 9);
            receiveOptions = frameData[11];
            const size_t payloadSize = frameLength - 12;
            _inputPayloadSize =
                payloadSize <= PayloadCapacity ? payloadSize : PayloadCapacity;
            _inputPayloadReadIndex = 0;
            for (size_t i = 0; i < _inputPayloadSize; ++i)
            {
                _inputPayload[i] = frameData[12 + i];
            }
            return byte_codec::combineBytes(api_frame::type::receivePacket,
                                            receiveOptions, 0x00, 0x00);
        }

        uint32_t _parseAtCommandResponse(const uint8_t *frameData,
                                         const size_t frameLength)
        {
            if (frameLength < 5)
            {
                return 0xFFFFFFFC;
            }

            frameId = frameData[1];
            atCommand = byte_codec::read16(frameData, 2);
            status = frameData[4];
            const size_t currentValueSize = frameLength - 5;
            _valueSize =
                currentValueSize <= ValueCapacity ? currentValueSize : ValueCapacity;
            for (size_t i = 0; i < _valueSize; ++i)
            {
                _value[i] = frameData[5 + i];
            }
            if (status == 0)
            {
                _applyAtValue(atCommand, _value, _valueSize);
            }
            frameIds.free(frameId);
            return byte_codec::combineBytes(api_frame::type::atCommandResponse, status,
                                            frameId, 0x00);
        }

        uint32_t _parseRemoteAtCommandResponse(const uint8_t *frameData,
                                               const size_t frameLength)
        {
            if (frameLength < 15)
            {
                return 0xFFFFFFFC;
            }

            frameId = frameData[1];
            xbee64Id = byte_codec::read64(frameData, 2);
            xbee16Id = byte_codec::read16(frameData, 10);
            atCommand = byte_codec::read16(frameData, 12);
            status = frameData[14];
            const size_t currentValueSize = frameLength - 15;
            _valueSize =
                currentValueSize <= ValueCapacity ? currentValueSize : ValueCapacity;
            for (size_t i = 0; i < _valueSize; ++i)
            {
                _value[i] = frameData[15 + i];
            }
            frameIds.free(frameId);
            return byte_codec::combineBytes(api_frame::type::remoteAtCommandResponse,
                                            status, frameId, 0x00);
        }

        uint32_t _parseModemStatus(const uint8_t *frameData,
                                   const size_t frameLength)
        {
            if (frameLength < 2)
            {
                return 0xFFFFFFFC;
            }

            status = frameData[1];
            if (status == 0x02)
            {
                parameters.ai = 1;
            }
            else if (status == 0x03)
            {
                parameters.ai = 0;
            }
            return byte_codec::combineBytes(api_frame::type::modemStatus, status, 0x00,
                                            0x00);
        }

        uint32_t _parseTransmitStatus(const uint8_t *frameData,
                                      const size_t frameLength)
        {
            if (frameLength < 7)
            {
                return 0xFFFFFFFC;
            }

            frameId = frameData[1];
            xbee16Id = byte_codec::read16(frameData, 2);
            status = frameData[5];
            discoveryStatus = frameData[6];
            if (status == 0)
            {
                frameIds.free(frameId);
            }
            return byte_codec::combineBytes(api_frame::type::transmitStatus, status,
                                            frameId, discoveryStatus);
        }

        void _applyAtValue(const uint16_t command, const uint8_t *value,
                           const size_t valueSize)
        {
            if (valueSize == 1)
            {
                _applyAtValue8(command, value[0]);
            }
            else if (valueSize == 2)
            {
                _applyAtValue16(command, byte_codec::read16(value));
            }
            else if (valueSize == 4)
            {
                _applyAtValue32(command, byte_codec::read32(value));
            }
            else if (valueSize == 8)
            {
                _applyAtValue64(command, byte_codec::read64(value));
            }
        }

        void _applyAtValue8(const uint16_t command, const uint8_t value)
        {
            switch (command)
            {
            case at_command::sd:
                parameters.sd = value;
                break;
            case at_command::zs:
                parameters.zs = value;
                break;
            case at_command::nj:
                parameters.nj = value;
                break;
            case at_command::jv:
                parameters.jv = value;
                break;
            case at_command::jn:
                parameters.jn = value;
                break;
            case at_command::ch:
                parameters.ch = value;
                break;
            case at_command::nc:
                parameters.nc = value;
                break;
            case at_command::ni:
                parameters.ni = value;
                break;
            case at_command::nh:
                parameters.nh = value;
                break;
            case at_command::bh:
                parameters.bh = value;
                break;
            case at_command::ar:
                parameters.ar = value;
                break;
            case at_command::nt:
                parameters.nt = value;
                break;
            case at_command::no:
                parameters.no = value;
                break;
            case at_command::np:
                parameters.np = value;
                break;
            case at_command::cr:
                parameters.cr = value;
                break;
            case at_command::pl:
                parameters.pl = value;
                break;
            case at_command::pm:
                parameters.pm = value;
                break;
            case at_command::pp:
                parameters.pp = value;
                break;
            case at_command::ee:
                parameters.ee = value;
                break;
            case at_command::eo:
                parameters.eo = value;
                break;
            case at_command::ky:
                parameters.ky = value;
                break;
            case at_command::bd:
                parameters.bd = value;
                break;
            case at_command::nb:
                parameters.nb = value;
                break;
            case at_command::sb:
                parameters.sb = value;
                break;
            case at_command::d7:
                parameters.d7 = value;
                break;
            case at_command::d6:
                parameters.d6 = value;
                break;
            case at_command::ap:
                parameters.ap = value;
                break;
            case at_command::ao:
                parameters.ao = value;
                break;
            case at_command::sm:
                parameters.sm = value;
                break;
            case at_command::so:
                parameters.so = value;
                break;
            case at_command::ai:
                parameters.ai = value;
                break;
            case at_command::db:
                parameters.db = value;
                break;
            default:
                break;
            }
        }

        void _applyAtValue16(const uint16_t command, const uint16_t value)
        {
            switch (command)
            {
            case at_command::sc:
                parameters.sc = value;
                break;
            case at_command::nw:
                parameters.nw = value;
                break;
            case at_command::oi:
                parameters.oi = value;
                break;
            case at_command::my:
                parameters.my = value;
                break;
            case at_command::sn:
                parameters.sn = value;
                break;
            case at_command::sp:
                parameters.sp = value;
                break;
            case at_command::st:
                parameters.st = value;
                break;
            case at_command::po:
                parameters.po = value;
                break;
            case at_command::vr:
                parameters.vr = value;
                break;
            case at_command::hv:
                parameters.hv = value;
                break;
            case at_command::v:
                parameters.v = value;
                break;
            case at_command::nr:
                parameters.nr = value;
                break;
            case at_command::wr:
                parameters.wr = value;
                break;
            default:
                break;
            }
        }

        void _applyAtValue32(const uint16_t command, const uint32_t value)
        {
            if (command == at_command::dd)
            {
                parameters.dd = value;
            }
            else if (command == at_command::sl)
            {
                parameters.sl = value;
            }
        }

        void _applyAtValue64(const uint16_t command, const uint64_t value)
        {
            switch (command)
            {
            case at_command::id:
                parameters.id = value;
                break;
            case at_command::op:
                parameters.op = value;
                break;
            case at_command::sa:
                parameters.sa = value;
                break;
            case at_command::da:
                parameters.da = value;
                break;
            default:
                break;
            }
        }
    };
}
