#include "serial_port/serial_port.hpp"

#include "core/byte_codec.hpp"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <thread>

namespace device_transport
{
    SerialPort::~SerialPort()
    {
        close();
    }

    TransportError SerialPort::open(const std::string &portName, const uint32_t baudRate)
    {
        close();

        if (portName.empty() || baudRate == 0)
        {
            return TransportError::invalidArgument;
        }

        HANDLE handle = CreateFileA(portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return TransportError::openFailed;
        }

        _nativeHandle = handle;

        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(handle, &dcb))
        {
            close();
            return TransportError::stateReadFailed;
        }

        dcb.BaudRate = static_cast<DWORD>(baudRate);
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        dcb.fBinary = TRUE;
        dcb.fParity = FALSE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDsrSensitivity = FALSE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        dcb.fAbortOnError = FALSE;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;

        if (!SetCommState(handle, &dcb))
        {
            close();
            return TransportError::configureFailed;
        }

        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        if (!SetCommTimeouts(handle, &timeouts))
        {
            close();
            return TransportError::timeoutConfigureFailed;
        }

        {
            std::lock_guard<std::mutex> inputLock(_inputMutex);
            _inputBuffer.clear();

            std::lock_guard<std::mutex> outputLock(_outputMutex);
            _outputBuffer.clear();
        }

        _running = true;
        _readerThread = std::thread(&SerialPort::_readerLoop, this);
        return TransportError::ok;
    }

    bool SerialPort::isOpen() const
    {
        return _nativeHandle != nullptr;
    }

    void SerialPort::close()
    {
        _running = false;
        _inputCondition.notify_all();

        if (_nativeHandle != nullptr)
        {
            HANDLE handle = static_cast<HANDLE>(_nativeHandle);
            PurgeComm(handle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
        }

        if (_readerThread.joinable())
        {
            _readerThread.join();
        }

        if (_nativeHandle != nullptr)
        {
            CloseHandle(static_cast<HANDLE>(_nativeHandle));
            _nativeHandle = nullptr;
        }
    }

    uint16_t SerialPort::bytesToRead() const
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        return static_cast<uint16_t>(_inputBuffer.size());
    }

    uint16_t SerialPort::bytesToWrite() const
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        return static_cast<uint16_t>(_outputBuffer.size());
    }

    uint32_t SerialPort::bytesInDriverQueue() const
    {
        if (_nativeHandle == nullptr)
        {
            return 0;
        }

        DWORD errors = 0;
        COMSTAT status{};
        ClearCommError(static_cast<HANDLE>(_nativeHandle), &errors, &status);
        return static_cast<uint32_t>(status.cbInQue);
    }

    bool SerialPort::waitForInputSize(const size_t byteCount, const uint32_t timeoutMs)
    {
        std::unique_lock<std::mutex> lock(_inputMutex);
        if (timeoutMs == 0)
        {
            _inputCondition.wait(lock, [this, byteCount]
                                 { return _inputBuffer.size() >= byteCount || !_running; });
            return _inputBuffer.size() >= byteCount;
        }

        return _inputCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, byteCount]
                                        { return _inputBuffer.size() >= byteCount || !_running; }) &&
               _inputBuffer.size() >= byteCount;
    }

    uint8_t SerialPort::read8()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        const uint8_t value = byte_codec::read8(_inputBuffer);
        _inputBuffer.erase(_inputBuffer.begin());
        return value;
    }

    uint16_t SerialPort::read16()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        const uint16_t value = byte_codec::read16(_inputBuffer);
        _inputBuffer.erase(_inputBuffer.begin(), _inputBuffer.begin() + 2);
        return value;
    }

    uint32_t SerialPort::read32()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        const uint32_t value = byte_codec::read32(_inputBuffer);
        _inputBuffer.erase(_inputBuffer.begin(), _inputBuffer.begin() + 4);
        return value;
    }

    uint64_t SerialPort::read64()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        const uint64_t value = byte_codec::read64(_inputBuffer);
        _inputBuffer.erase(_inputBuffer.begin(), _inputBuffer.begin() + 8);
        return value;
    }

    uint32_t SerialPort::write8(const uint8_t value)
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        byte_codec::write8(_outputBuffer, value);
        return 1;
    }

    uint32_t SerialPort::write16(const uint16_t value)
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        byte_codec::write16(_outputBuffer, value);
        return 2;
    }

    uint32_t SerialPort::write32(const uint32_t value)
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        byte_codec::write32(_outputBuffer, value);
        return 4;
    }

    uint32_t SerialPort::write64(const uint64_t value)
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        byte_codec::write64(_outputBuffer, value);
        return 8;
    }

    void SerialPort::clearInputBuffer()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        _inputBuffer.clear();
    }

    void SerialPort::clearOutputBuffer()
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        _outputBuffer.clear();
    }

    std::vector<uint8_t> SerialPort::getInputBuffer() const
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        return _inputBuffer;
    }

    std::vector<uint8_t> SerialPort::getOutputBuffer() const
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        return _outputBuffer;
    }

    uint32_t SerialPort::send()
    {
        if (_nativeHandle == nullptr)
        {
            return 0;
        }

        std::vector<uint8_t> bytes;
        {
            std::lock_guard<std::mutex> lock(_outputMutex);
            bytes.swap(_outputBuffer);
        }

        uint32_t totalWritten = 0;
        while (totalWritten < bytes.size())
        {
            DWORD bytesWritten = 0;
            const DWORD remaining = static_cast<DWORD>(bytes.size() - totalWritten);
            if (!WriteFile(static_cast<HANDLE>(_nativeHandle), bytes.data() + totalWritten, remaining, &bytesWritten, nullptr) || bytesWritten == 0)
            {
                break;
            }

            totalWritten += bytesWritten;
        }

        return totalWritten;
    }

    void SerialPort::_readerLoop()
    {
        std::vector<uint8_t> chunk(256, 0);

        while (_running)
        {
            const uint32_t queuedBytes = bytesInDriverQueue();
            if (queuedBytes == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            DWORD receivedSize = 0;
            const DWORD bytesToRead = static_cast<DWORD>(std::min<size_t>(chunk.size(), queuedBytes));
            if (!ReadFile(static_cast<HANDLE>(_nativeHandle), chunk.data(), bytesToRead, &receivedSize, nullptr))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (receivedSize > 0)
            {
                {
                    std::lock_guard<std::mutex> lock(_inputMutex);
                    _inputBuffer.insert(_inputBuffer.end(), chunk.begin(), chunk.begin() + static_cast<size_t>(receivedSize));
                }
                _inputCondition.notify_one();
            }
        }
    }
}
