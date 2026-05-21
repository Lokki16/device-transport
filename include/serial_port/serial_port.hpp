#pragma once

#include "core/error.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace device_transport
{
    class SerialPort
    {
    public:
        SerialPort() = default;
        ~SerialPort();

        TransportError open(const std::string &portName, uint32_t baudRate);
        bool isOpen() const;
        void close();

        uint16_t bytesToRead() const;
        uint16_t bytesToWrite() const;
        uint32_t bytesInDriverQueue() const;
        bool waitForInputSize(size_t byteCount, uint32_t timeoutMs);

        uint8_t read8();
        uint16_t read16();
        uint32_t read32();
        uint64_t read64();

        uint32_t write8(uint8_t value);
        uint32_t write16(uint16_t value);
        uint32_t write32(uint32_t value);
        uint32_t write64(uint64_t value);

        void clearInputBuffer();
        void clearOutputBuffer();

        std::vector<uint8_t> getInputBuffer() const;
        std::vector<uint8_t> getOutputBuffer() const;

        uint32_t send();

    private:
#ifdef _WIN32
        void *_nativeHandle = nullptr;
#else
        int _fileDescriptor = -1;
#endif

        std::atomic<bool> _running{false};
        std::thread _readerThread;

        std::vector<uint8_t> _inputBuffer;
        std::vector<uint8_t> _outputBuffer;
        mutable std::mutex _inputMutex;
        mutable std::mutex _outputMutex;
        std::condition_variable _inputCondition;

        void _readerLoop();
    };
}
