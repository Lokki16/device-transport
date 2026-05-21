#pragma once

#include "serial_port/serial_port.hpp"
#include "xbee/frames.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace device_transport
{
    class XBee
    {
    public:
        XBee() = default;
        ~XBee();

        TransportError open(const std::string &portName, uint32_t baudRate);
        bool isOpen() const;
        void close();

        uint8_t openNetwork(uint8_t seconds = 60);
        uint8_t closeNetwork();

        bool readPanId(uint16_t &panId, uint32_t timeoutMs = 200);
        bool readExtendedPanId(uint64_t &extendedPanId, uint32_t timeoutMs = 200);

        uint8_t atCommandRequest(uint16_t atCommand);
        uint8_t atCommandRequest(uint16_t atCommand, uint8_t value);
        uint8_t atCommandRequest(uint16_t atCommand, uint16_t value);
        uint8_t atCommandRequest(uint16_t atCommand, uint32_t value);
        uint8_t atCommandRequest(uint16_t atCommand, uint64_t value);

        uint8_t remoteAtCommandRequest(uint64_t destinationSn, uint16_t atCommand, uint16_t destinationNa = 0xFFFE, uint8_t rco = 0x02);
        uint8_t remoteAtCommandRequest(uint64_t destinationSn, uint16_t atCommand, uint8_t value, uint16_t destinationNa = 0xFFFE, uint8_t rco = 0x02);
        uint8_t remoteAtCommandRequest(uint64_t destinationSn, uint16_t atCommand, uint16_t value, uint16_t destinationNa = 0xFFFE, uint8_t rco = 0x02);
        uint8_t remoteAtCommandRequest(uint64_t destinationSn, uint16_t atCommand, uint32_t value, uint16_t destinationNa = 0xFFFE, uint8_t rco = 0x02);
        uint8_t remoteAtCommandRequest(uint64_t destinationSn, uint16_t atCommand, uint64_t value, uint16_t destinationNa = 0xFFFE, uint8_t rco = 0x02);

        uint8_t transmitRequest(uint64_t destinationSn, uint16_t destinationNa = 0xFFFE, uint8_t broadcastRadius = 0x00, uint8_t options = 0x00);
        uint8_t transmitRequest(uint64_t destinationSn, const std::vector<uint8_t> &payload, uint16_t destinationNa = 0xFFFE, uint8_t broadcastRadius = 0x00, uint8_t options = 0x00);

        void clearOutputPayload();

        void writeToOutputPayload(uint8_t input);
        void writeToOutputPayload(uint16_t input);
        void writeToOutputPayload(uint32_t input);
        void writeToOutputPayload(uint64_t input);

        std::vector<ReceivedXBeeFrame> getParsedInputPayload();
        std::vector<ReceivedXBeeFrame> waitAndTakeParsedInputPayload(uint32_t timeoutMs = 0);
        void interruptParsedInputPayloadWait();
        std::vector<AtCommandResponse> getParsedAtCommandResponses();
        std::vector<RemoteAtCommandResponse> getParsedRemoteAtCommandResponses();
        std::vector<TransmitStatus> getParsedTransmitStatuses();
        std::vector<ModemStatus> getParsedModemStatuses();

        static uint8_t calculateChecksum(const std::vector<uint8_t> &frameData);

    private:
        SerialPort _serialPort;

        std::atomic<bool> _running{false};
        std::thread _parserThread;

        std::vector<uint8_t> _frameData;
        std::vector<uint8_t> _outputPayload;
        std::vector<ReceivedXBeeFrame> _parsedPayloads;
        std::vector<AtCommandResponse> _parsedAtCommandResponses;
        std::vector<RemoteAtCommandResponse> _parsedRemoteAtCommandResponses;
        std::vector<TransmitStatus> _parsedTransmitStatuses;
        std::vector<ModemStatus> _parsedModemStatuses;
        mutable std::mutex _outputPayloadMutex;
        mutable std::mutex _parsedPayloadMutex;
        std::condition_variable _parsedPayloadCondition;
        bool _parsedPayloadWaitInterrupted{};
        std::mutex _commandMutex;
        std::mutex _atResponseMutex;
        std::condition_variable _atResponseCondition;

        uint8_t _nextFrameId{1};
        bool _pendingAtResponse{};
        bool _pendingAtResponseCompleted{};
        uint8_t _pendingAtFrameId{};
        uint16_t _pendingAtCommand{};
        uint8_t _pendingAtStatus{0xFF};
        std::vector<uint8_t> _pendingAtData;

        void _clearFrameData();
        uint8_t _nextAtFrameId();
        uint8_t _nextFrameIdForRequest(bool assignId);
        bool _readAtCommandData(uint16_t atCommand, std::vector<uint8_t> &data, uint32_t timeoutMs);

        static uint8_t _calculateChecksum(const std::vector<uint8_t> &frameData);

        uint8_t _sendFrameData();
        void _parserLoop();
    };
}
