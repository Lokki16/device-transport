#include "serial_port/xbee/xbee.hpp"

#include "core/byte_codec.hpp"
#include "serial_port/xbee/protocol.hpp"

#include <chrono>
#include <thread>
#include <utility>

namespace device_transport
{
    namespace
    {
        constexpr uint8_t remoteCommandOptions = 0x02;
    }

    XBee::~XBee()
    {
        close();
    }

    TransportError XBee::open(const std::string &portName, const uint32_t baudRate, const api_frame::ApiMode apiMode)
    {
        close();

        const TransportError openResult = _serialPort.open(portName, baudRate);
        if (openResult != TransportError::ok)
        {
            return openResult;
        }

        {
            std::lock_guard<std::mutex> outputLock(_outputPayloadMutex);
            _outputPayload.clear();

            std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
            _parsedPayloads.clear();
            _parsedPayloadWaitInterrupted = false;
        }

        {
            std::lock_guard<std::mutex> responseLock(_atResponseMutex);
            _pendingAtResponse = false;
            _pendingAtResponseCompleted = false;
            _pendingAtFrameId = 0;
            _pendingAtCommand = 0;
            _pendingAtStatus = 0xFF;
            _pendingAtData.clear();
        }

        _clearFrameData();
        _apiMode = apiMode;
        _running = true;
        _parserThread = std::thread(&XBee::_parserLoop, this);
        return TransportError::ok;
    }

    bool XBee::isOpen() const
    {
        return _serialPort.isOpen();
    }

    void XBee::close()
    {
        _running = false;
        interruptParsedInputPayloadWait();
        _atResponseCondition.notify_all();
        _serialPort.close();

        if (_parserThread.joinable())
        {
            _parserThread.join();
        }
    }

    uint8_t XBee::openNetwork(const uint8_t seconds)
    {
        const uint8_t nodeJoinResult = atCommandRequest(at_command::nj, seconds);
        if (nodeJoinResult != 0)
        {
            return nodeJoinResult;
        }

        const uint8_t applyResult = atCommandRequest(at_command::ac);
        if (applyResult != 0)
        {
            return applyResult;
        }

        return atCommandRequest(at_command::cb, static_cast<uint8_t>(2));
    }

    uint8_t XBee::closeNetwork()
    {
        const uint8_t nodeJoinResult = atCommandRequest(at_command::nj, static_cast<uint8_t>(0));
        if (nodeJoinResult != 0)
        {
            return nodeJoinResult;
        }

        return atCommandRequest(at_command::ac);
    }

    bool XBee::readAtCommandData(const uint16_t atCommand, std::vector<uint8_t> &data, const uint32_t timeoutMs)
    {
        uint8_t frameId = 0;
        {
            std::lock_guard<std::mutex> responseLock(_atResponseMutex);
            if (_pendingAtResponse)
            {
                return false;
            }

            frameId = _nextFrameIdForRequest();
            _pendingAtResponse = true;
            _pendingAtResponseCompleted = false;
            _pendingAtFrameId = frameId;
            _pendingAtCommand = atCommand;
            _pendingAtStatus = 0xFF;
            _pendingAtData.clear();
        }

        {
            std::lock_guard<std::mutex> commandLock(_commandMutex);
            _clearFrameData();
            byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
            byte_codec::write8(_frameData, frameId);
            byte_codec::write16(_frameData, atCommand);
            _sendFrameData();
        }

        std::unique_lock<std::mutex> responseLock(_atResponseMutex);
        const bool success = _atResponseCondition.wait_for(responseLock, std::chrono::milliseconds(timeoutMs), [this]
                                                           { return _pendingAtResponseCompleted || !_running; }) &&
                             _pendingAtResponseCompleted &&
                             _pendingAtStatus == 0;

        if (success)
        {
            data = _pendingAtData;
        }

        _pendingAtResponse = false;
        _pendingAtResponseCompleted = false;
        _pendingAtData.clear();
        return success;
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand)
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        return _sendFrameData();
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint8_t value)
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write8(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint16_t value)
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write16(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint32_t value)
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write32(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint64_t value)
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write64(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint16_t destinationNa)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, remoteCommandOptions);
        byte_codec::write16(_frameData, atCommand);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint8_t value, const uint16_t destinationNa)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, remoteCommandOptions);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write8(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint16_t value, const uint16_t destinationNa)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, remoteCommandOptions);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write16(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint32_t value, const uint16_t destinationNa)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, remoteCommandOptions);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write32(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint64_t value, const uint16_t destinationNa)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, remoteCommandOptions);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write64(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::transmitRequest(const uint64_t destinationSn, const uint16_t destinationNa, const uint8_t broadcastRadius, const uint8_t options)
    {
        std::vector<uint8_t> payload;
        {
            std::lock_guard<std::mutex> lock(_outputPayloadMutex);
            payload.swap(_outputPayload);
        }

        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::transmitRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, broadcastRadius);
        byte_codec::write8(_frameData, options);
        for (const uint8_t byte : payload)
        {
            byte_codec::write8(_frameData, byte);
        }

        return _sendFrameData();
    }

    void XBee::clearOutputPayload()
    {
        std::lock_guard<std::mutex> lock(_outputPayloadMutex);
        _outputPayload.clear();
    }

    void XBee::write8(const uint8_t input)
    {
        std::lock_guard<std::mutex> lock(_outputPayloadMutex);
        byte_codec::write8(_outputPayload, input);
    }

    void XBee::write16(const uint16_t input)
    {
        std::lock_guard<std::mutex> lock(_outputPayloadMutex);
        byte_codec::write16(_outputPayload, input);
    }

    void XBee::write32(const uint32_t input)
    {
        std::lock_guard<std::mutex> lock(_outputPayloadMutex);
        byte_codec::write32(_outputPayload, input);
    }

    void XBee::write64(const uint64_t input)
    {
        std::lock_guard<std::mutex> lock(_outputPayloadMutex);
        byte_codec::write64(_outputPayload, input);
    }

    std::vector<ReceivedXBeeFrame> XBee::getParsedInputPayload()
    {
        std::lock_guard<std::mutex> lock(_parsedPayloadMutex);
        std::vector<ReceivedXBeeFrame> payloads;
        payloads.swap(_parsedPayloads);
        return payloads;
    }

    std::vector<ReceivedXBeeFrame> XBee::waitAndTakeParsedInputPayload(const uint32_t timeoutMs)
    {
        std::unique_lock<std::mutex> lock(_parsedPayloadMutex);
        const auto canStopWaiting = [this]
        {
            return !_parsedPayloads.empty() || !_running || _parsedPayloadWaitInterrupted;
        };

        if (timeoutMs == 0)
        {
            _parsedPayloadCondition.wait(lock, canStopWaiting);
        }
        else
        {
            _parsedPayloadCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), canStopWaiting);
        }

        std::vector<ReceivedXBeeFrame> payloads;
        payloads.swap(_parsedPayloads);
        _parsedPayloadWaitInterrupted = false;
        return payloads;
    }

    void XBee::interruptParsedInputPayloadWait()
    {
        {
            std::lock_guard<std::mutex> lock(_parsedPayloadMutex);
            _parsedPayloadWaitInterrupted = true;
        }
        _parsedPayloadCondition.notify_all();
    }

    void XBee::setTraceCallback(const XBeeTraceCallback callback, void *userData)
    {
        _traceCallback = callback;
        _traceUserData = userData;
    }

    void XBee::_clearFrameData()
    {
        _frameData.clear();
    }

    uint8_t XBee::_nextFrameIdForRequest()
    {
        if (_nextFrameId == 0 || _nextFrameId == 0xFF)
        {
            _nextFrameId = 1;
        }

        const uint8_t frameId = _nextFrameId++;
        if (_nextFrameId == 0xFF)
        {
            _nextFrameId = 1;
        }
        return frameId;
    }

    uint8_t XBee::_sendFrameData()
    {
        std::vector<uint8_t> frame(_frameData.size() + 4);
        size_t frameSize = 0;
        if (!xbee_protocol::buildFrame(frame.data(), frame.size(), frameSize, _frameData.data(), _frameData.size()))
        {
            return 1;
        }

        std::vector<uint8_t> output;
        output.reserve(frameSize * 2);
        output.push_back(api_frame::startDelimiter);
        for (size_t i = 1; i < frameSize; ++i)
        {
            _appendEscapedByte(output, frame[i]);
        }

        _serialPort.clearOutputBuffer();
        for (const uint8_t byte : output)
        {
            _serialPort.write8(byte);
        }

        const uint32_t writtenSize = _serialPort.send();
        _trace(XBeeTraceDirection::tx, output.data(), output.size());
        return writtenSize == output.size() ? 0 : 1;
    }

    void XBee::_appendEscapedByte(std::vector<uint8_t> &output, const uint8_t byte) const
    {
        if (_apiMode == api_frame::ApiMode::api2 &&
            (byte == api_frame::startDelimiter || byte == api_frame::escape || byte == api_frame::xon || byte == api_frame::xoff))
        {
            output.push_back(api_frame::escape);
            output.push_back(static_cast<uint8_t>(byte ^ 0x20));
            return;
        }

        output.push_back(byte);
    }

    void XBee::_trace(const XBeeTraceDirection direction, const uint8_t *bytes, const size_t size) const
    {
        if (_traceCallback != nullptr && bytes != nullptr && size != 0)
        {
            _traceCallback(direction, bytes, size, _traceUserData);
        }
    }

    void XBee::_parserLoop()
    {
        xbee_protocol::FrameParser<1024> parser;
        bool insideFrame = false;
        bool escapeNext = false;

        while (_running)
        {
            if (!_serialPort.waitForInputSize(1, 0))
            {
                continue;
            }

            while (_running && _serialPort.bytesToRead() > 0)
            {
                uint8_t byte = _serialPort.read8();
                if (byte == api_frame::startDelimiter)
                {
                    if (_apiMode == api_frame::ApiMode::api2)
                    {
                        parser.reset();
                    }
                    insideFrame = true;
                    escapeNext = false;
                }
                else if (_apiMode == api_frame::ApiMode::api2 && insideFrame)
                {
                    if (escapeNext)
                    {
                        byte = static_cast<uint8_t>(byte ^ 0x20);
                        escapeNext = false;
                    }
                    else if (byte == api_frame::escape)
                    {
                        escapeNext = true;
                        continue;
                    }
                }

                xbee_protocol::FrameView frame;
                const xbee_protocol::ParseStatus status = parser.process(byte, frame);
                if (status == xbee_protocol::ParseStatus::checksumError || status == xbee_protocol::ParseStatus::frameTooLarge)
                {
                    insideFrame = false;
                    escapeNext = false;
                    continue;
                }

                if (status != xbee_protocol::ParseStatus::frameReady || frame.size == 0)
                {
                    continue;
                }

                insideFrame = false;
                escapeNext = false;

                const uint8_t frameType = frame.data[0];
                std::vector<uint8_t> rawFrame(frame.size + 4);
                size_t rawFrameSize = 0;
                if (xbee_protocol::buildFrame(rawFrame.data(), rawFrame.size(), rawFrameSize, frame.data, frame.size))
                {
                    rawFrame.resize(rawFrameSize);
                    _trace(XBeeTraceDirection::rx, rawFrame.data(), rawFrame.size());
                }
                else
                {
                    _trace(XBeeTraceDirection::rx, frame.data, frame.size);
                }

                if (frameType == api_frame::type::atCommandResponse && frame.size >= 5)
                {
                    const uint8_t frameId = frame.data[1];
                    const uint16_t atCommand = byte_codec::read16(frame.data, 2);
                    const uint8_t commandStatus = frame.data[4];
                    std::vector<uint8_t> commandData(frame.data + 5, frame.data + frame.size);
                    {
                        std::lock_guard<std::mutex> responseLock(_atResponseMutex);
                        if (_pendingAtResponse && _pendingAtFrameId == frameId && _pendingAtCommand == atCommand)
                        {
                            _pendingAtStatus = commandStatus;
                            _pendingAtData = std::move(commandData);
                            _pendingAtResponseCompleted = true;
                            _atResponseCondition.notify_all();
                        }
                    }

                    continue;
                }

                if (frameType == api_frame::type::receivePacket && frame.size >= 12)
                {
                    ReceivedXBeeFrame payload;
                    payload.xbee64Id = byte_codec::read64(frame.data, 1);
                    payload.xbee16Id = byte_codec::read16(frame.data, 9);
                    payload.receiveOptions = frame.data[11];
                    payload.payload.assign(frame.data + 12, frame.data + frame.size);
                    {
                        std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
                        _parsedPayloads.push_back(std::move(payload));
                    }
                    _parsedPayloadCondition.notify_one();
                    continue;
                }

                if (frameType == api_frame::type::explicitReceiveIndicator && frame.size >= 18)
                {
                    ReceivedXBeeFrame payload;
                    payload.xbee64Id = byte_codec::read64(frame.data, 1);
                    payload.xbee16Id = byte_codec::read16(frame.data, 9);
                    payload.receiveOptions = frame.data[17];
                    payload.payload.assign(frame.data + 18, frame.data + frame.size);
                    {
                        std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
                        _parsedPayloads.push_back(std::move(payload));
                    }
                    _parsedPayloadCondition.notify_one();
                    continue;
                }
            }
        }
    }
}
