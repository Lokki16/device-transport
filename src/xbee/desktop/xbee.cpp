#include "xbee/desktop/xbee.hpp"

#include "core/byte_codec.hpp"
#include "xbee/protocol.hpp"

#include <chrono>
#include <thread>
#include <utility>

namespace device_transport
{
    XBee::~XBee()
    {
        close();
    }

    TransportError XBee::open(const std::string &portName, const uint32_t baudRate)
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
            _parsedAtCommandResponses.clear();
            _parsedRemoteAtCommandResponses.clear();
            _parsedTransmitStatuses.clear();
            _parsedModemStatuses.clear();
        }

        _clearFrameData();
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
        _parsedPayloadCondition.notify_all();
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

    bool XBee::readPanId(uint16_t &panId, const uint32_t timeoutMs)
    {
        std::vector<uint8_t> data;
        if (!_readAtCommandData(at_command::oi, data, timeoutMs) || data.size() != 2)
        {
            return false;
        }

        panId = byte_codec::read16(data);
        return true;
    }

    bool XBee::readExtendedPanId(uint64_t &extendedPanId, const uint32_t timeoutMs)
    {
        std::vector<uint8_t> data;
        if (!_readAtCommandData(at_command::op, data, timeoutMs) || data.size() != 8)
        {
            return false;
        }

        extendedPanId = byte_codec::read64(data);
        return true;
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        return _sendFrameData();
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint8_t value)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write8(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint16_t value)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write16(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint32_t value)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write32(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint64_t value)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::atCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write64(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint16_t destinationNa, const uint8_t rco)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, rco);
        byte_codec::write16(_frameData, atCommand);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint8_t value, const uint16_t destinationNa, const uint8_t rco)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, rco);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write8(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint16_t value, const uint16_t destinationNa, const uint8_t rco)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, rco);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write16(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint32_t value, const uint16_t destinationNa, const uint8_t rco)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, rco);
        byte_codec::write16(_frameData, atCommand);
        byte_codec::write32(_frameData, value);
        return _sendFrameData();
    }

    uint8_t XBee::remoteAtCommandRequest(const uint64_t destinationSn, const uint16_t atCommand, const uint64_t value, const uint16_t destinationNa, const uint8_t rco)
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        byte_codec::write8(_frameData, api_frame::type::remoteAtCommandRequest);
        byte_codec::write8(_frameData, api_frame::defaultFrameId);
        byte_codec::write64(_frameData, destinationSn);
        byte_codec::write16(_frameData, destinationNa);
        byte_codec::write8(_frameData, rco);
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

        return transmitRequest(destinationSn, payload, destinationNa, broadcastRadius, options);
    }

    uint8_t XBee::transmitRequest(const uint64_t destinationSn, const std::vector<uint8_t> &payload, const uint16_t destinationNa, const uint8_t broadcastRadius, const uint8_t options)
    {
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

    void XBee::writeToOutputPayload(const uint8_t input)
    {
        std::lock_guard<std::mutex> lock(_outputPayloadMutex);
        byte_codec::write8(_outputPayload, input);
    }

    void XBee::writeToOutputPayload(const uint16_t input)
    {
        std::lock_guard<std::mutex> lock(_outputPayloadMutex);
        byte_codec::write16(_outputPayload, input);
    }

    void XBee::writeToOutputPayload(const uint32_t input)
    {
        std::lock_guard<std::mutex> lock(_outputPayloadMutex);
        byte_codec::write32(_outputPayload, input);
    }

    void XBee::writeToOutputPayload(const uint64_t input)
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
        _parsedPayloadCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]
                                         { return !_parsedPayloads.empty() || !_running; });

        std::vector<ReceivedXBeeFrame> payloads;
        payloads.swap(_parsedPayloads);
        return payloads;
    }

    std::vector<AtCommandResponse> XBee::getParsedAtCommandResponses()
    {
        std::lock_guard<std::mutex> lock(_parsedPayloadMutex);
        std::vector<AtCommandResponse> responses;
        responses.swap(_parsedAtCommandResponses);
        return responses;
    }

    std::vector<RemoteAtCommandResponse> XBee::getParsedRemoteAtCommandResponses()
    {
        std::lock_guard<std::mutex> lock(_parsedPayloadMutex);
        std::vector<RemoteAtCommandResponse> responses;
        responses.swap(_parsedRemoteAtCommandResponses);
        return responses;
    }

    std::vector<TransmitStatus> XBee::getParsedTransmitStatuses()
    {
        std::lock_guard<std::mutex> lock(_parsedPayloadMutex);
        std::vector<TransmitStatus> statuses;
        statuses.swap(_parsedTransmitStatuses);
        return statuses;
    }

    std::vector<ModemStatus> XBee::getParsedModemStatuses()
    {
        std::lock_guard<std::mutex> lock(_parsedPayloadMutex);
        std::vector<ModemStatus> statuses;
        statuses.swap(_parsedModemStatuses);
        return statuses;
    }

    uint8_t XBee::calculateChecksum(const std::vector<uint8_t> &frameData)
    {
        return _calculateChecksum(frameData);
    }

    void XBee::_clearFrameData()
    {
        _frameData.clear();
    }

    uint8_t XBee::_nextAtFrameId()
    {
        return _nextFrameIdForRequest(true);
    }

    uint8_t XBee::_nextFrameIdForRequest(const bool assignId)
    {
        if (!assignId)
        {
            return 0;
        }

        if (_nextFrameId == 0)
        {
            _nextFrameId = 1;
        }

        return _nextFrameId++;
    }

    bool XBee::_readAtCommandData(const uint16_t atCommand, std::vector<uint8_t> &data, const uint32_t timeoutMs)
    {
        uint8_t frameId = 0;
        {
            std::lock_guard<std::mutex> responseLock(_atResponseMutex);
            if (_pendingAtResponse)
            {
                return false;
            }

            frameId = _nextAtFrameId();
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

    uint8_t XBee::_calculateChecksum(const std::vector<uint8_t> &frameData)
    {
        return xbee_protocol::calculateChecksum(frameData.data(), frameData.size());
    }

    uint8_t XBee::_sendFrameData()
    {
        std::vector<uint8_t> frame(_frameData.size() + 4);
        size_t frameSize = 0;
        if (!xbee_protocol::buildFrame(frame.data(), frame.size(), frameSize, _frameData.data(), _frameData.size()))
        {
            return 1;
        }

        _serialPort.clearOutputBuffer();
        for (size_t i = 0; i < frameSize; ++i)
        {
            _serialPort.write8(frame[i]);
        }

        const uint32_t writtenSize = _serialPort.send();
        return writtenSize == frameSize ? 0 : 1;
    }

    void XBee::_parserLoop()
    {
        xbee_protocol::FrameParser<1024> parser;

        while (_running)
        {
            if (!_serialPort.waitForInputSize(1, 100))
            {
                continue;
            }

            while (_running && _serialPort.bytesToRead() > 0)
            {
                xbee_protocol::FrameView frame;
                const xbee_protocol::ParseStatus status = parser.process(_serialPort.read8(), frame);
                if (status != xbee_protocol::ParseStatus::frameReady || frame.size == 0)
                {
                    continue;
                }

                const uint8_t frameType = frame.data[0];

                if (frameType == api_frame::type::atCommandResponse && frame.size >= 5)
                {
                    AtCommandResponse response;
                    response.frameId = frame.data[1];
                    response.atCommand = byte_codec::read16(frame.data, 2);
                    response.status = frame.data[4];
                    response.value.assign(frame.data + 5, frame.data + frame.size);

                    {
                        std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
                        _parsedAtCommandResponses.push_back(response);
                    }

                    {
                        std::lock_guard<std::mutex> responseLock(_atResponseMutex);
                        if (_pendingAtResponse && _pendingAtFrameId == response.frameId && _pendingAtCommand == response.atCommand)
                        {
                            _pendingAtStatus = response.status;
                            _pendingAtData = response.value;
                            _pendingAtResponseCompleted = true;
                            _atResponseCondition.notify_all();
                        }
                    }
                    continue;
                }

                if (frameType == api_frame::type::modemStatus && frame.size >= 2)
                {
                    ModemStatus statusFrame;
                    statusFrame.status = frame.data[1];
                    {
                        std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
                        _parsedModemStatuses.push_back(statusFrame);
                    }
                    _parsedPayloadCondition.notify_one();
                    continue;
                }

                if (frameType == api_frame::type::transmitStatus && frame.size >= 7)
                {
                    TransmitStatus statusFrame;
                    statusFrame.frameId = frame.data[1];
                    statusFrame.destinationAddress = byte_codec::read16(frame.data, 2);
                    statusFrame.retryCount = frame.data[4];
                    statusFrame.deliveryStatus = frame.data[5];
                    statusFrame.discoveryStatus = frame.data[6];
                    {
                        std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
                        _parsedTransmitStatuses.push_back(statusFrame);
                    }
                    _parsedPayloadCondition.notify_one();
                    continue;
                }

                if (frameType == api_frame::type::remoteAtCommandResponse && frame.size >= 15)
                {
                    RemoteAtCommandResponse response;
                    response.frameId = frame.data[1];
                    response.xbee64Id = byte_codec::read64(frame.data, 2);
                    response.xbee16Id = byte_codec::read16(frame.data, 10);
                    response.atCommand = byte_codec::read16(frame.data, 12);
                    response.status = frame.data[14];
                    response.value.assign(frame.data + 15, frame.data + frame.size);
                    {
                        std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
                        _parsedRemoteAtCommandResponses.push_back(std::move(response));
                    }
                    _parsedPayloadCondition.notify_one();
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
            }
        }
    }
}
