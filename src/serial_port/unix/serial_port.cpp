#ifndef _WIN32

#include "serial_port/serial_port.hpp"

#include "core/byte_codec.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace
{
    speed_t toPosixBaudRate(const uint32_t baudRate)
    {
        switch (baudRate)
        {
        case 1200:
            return B1200;
        case 2400:
            return B2400;
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        default:
            return 0;
        }
    }
}

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

        _fileDescriptor = ::open(portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (_fileDescriptor < 0)
        {
            return TransportError::openFailed;
        }

        termios options{};
        if (tcgetattr(_fileDescriptor, &options) != 0)
        {
            close();
            return TransportError::stateReadFailed;
        }

        cfmakeraw(&options);
        const speed_t speed = toPosixBaudRate(baudRate);
        if (speed == 0)
        {
            close();
            return TransportError::unsupportedBaudRate;
        }

        cfsetispeed(&options, speed);
        cfsetospeed(&options, speed);
        options.c_cflag |= CLOCAL | CREAD;
        options.c_cflag &= ~CRTSCTS;
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;
        options.c_cc[VMIN] = 0;
        options.c_cc[VTIME] = 1;

        if (tcsetattr(_fileDescriptor, TCSANOW, &options) != 0)
        {
            close();
            return TransportError::configureFailed;
        }

        tcflush(_fileDescriptor, TCIOFLUSH);

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
        return _fileDescriptor >= 0;
    }

    void SerialPort::close()
    {
        _running = false;
        _inputCondition.notify_all();

        if (_readerThread.joinable())
        {
            _readerThread.join();
        }

        if (_fileDescriptor >= 0)
        {
            ::close(_fileDescriptor);
            _fileDescriptor = -1;
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
        if (_fileDescriptor < 0)
        {
            return 0;
        }

        int count = 0;
        if (ioctl(_fileDescriptor, FIONREAD, &count) != 0)
        {
            return 0;
        }
        return static_cast<uint32_t>(count);
    }

    bool SerialPort::waitForInputSize(const size_t byteCount, const uint32_t timeoutMs)
    {
        std::unique_lock<std::mutex> lock(_inputMutex);
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
        if (_fileDescriptor < 0)
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
            const ssize_t written = ::write(_fileDescriptor, bytes.data() + totalWritten, bytes.size() - totalWritten);
            if (written > 0)
            {
                totalWritten += static_cast<uint32_t>(written);
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            break;
        }

        return totalWritten;
    }

    void SerialPort::_readerLoop()
    {
        std::vector<uint8_t> chunk(256, 0);

        while (_running)
        {
            const ssize_t receivedSize = ::read(_fileDescriptor, chunk.data(), chunk.size());
            if (receivedSize > 0)
            {
                {
                    std::lock_guard<std::mutex> lock(_inputMutex);
                    _inputBuffer.insert(_inputBuffer.end(), chunk.begin(), chunk.begin() + static_cast<size_t>(receivedSize));
                }
                _inputCondition.notify_one();
                continue;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

#endif
