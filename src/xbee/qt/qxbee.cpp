#include "xbee/qt/qxbee.hpp"

#include "core/byte_codec.hpp"

namespace device_transport
{
    QXBee::QXBee(QObject *parent)
        : QObject(parent)
    {
        connect(&_port, &QSerialPort::readyRead, this, &QXBee::_onReadyRead);
    }

    bool QXBee::open(const QString &portName, const int baudRate, const ApiMode mode)
    {
        close();

        _port.setPortName(portName);
        _port.setBaudRate(baudRate);
        _port.setDataBits(QSerialPort::Data8);
        _port.setParity(QSerialPort::NoParity);
        _port.setStopBits(QSerialPort::OneStop);
        _port.setFlowControl(QSerialPort::NoFlowControl);

        if (!_port.open(QIODevice::ReadWrite))
        {
            emit errorOccurred(_port.errorString());
            return false;
        }

        _apiMode = mode;
        _insideFrame = false;
        _escapeNext = false;
        _parser.reset();
        return true;
    }

    void QXBee::close()
    {
        if (_port.isOpen())
        {
            _port.close();
        }
    }

    bool QXBee::isOpen() const
    {
        return _port.isOpen();
    }

    QXBee::ApiMode QXBee::apiMode() const
    {
        return _apiMode;
    }

    void QXBee::setApiMode(const ApiMode mode)
    {
        _apiMode = mode;
    }

    void QXBee::_onReadyRead()
    {
        const QByteArray data = _port.readAll();
        for (const unsigned char byte : data)
        {
            _processByte(byte);
        }
    }

    void QXBee::_processByte(quint8 byte)
    {
        if (byte == device_transport::api_frame::startDelimiter)
        {
            _insideFrame = true;
            _escapeNext = false;
        }
        else if (_apiMode == ApiMode::Api2 && _insideFrame)
        {
            if (_escapeNext)
            {
                byte ^= 0x20;
                _escapeNext = false;
            }
            else if (byte == device_transport::api_frame::escape)
            {
                _escapeNext = true;
                return;
            }
        }

        xbee_protocol::FrameView frame;
        const xbee_protocol::ParseStatus status = _parser.process(byte, frame);
        if (status == xbee_protocol::ParseStatus::checksumError)
        {
            emit errorOccurred(QStringLiteral("XBee checksum error"));
            _insideFrame = false;
            _escapeNext = false;
            return;
        }

        if (status == xbee_protocol::ParseStatus::frameTooLarge)
        {
            emit errorOccurred(QStringLiteral("XBee frame is too large"));
            _insideFrame = false;
            _escapeNext = false;
            return;
        }

        if (status != xbee_protocol::ParseStatus::frameReady || frame.size == 0)
        {
            return;
        }

        const quint8 frameType = frame.data[0];
        if (frameType == device_transport::api_frame::type::receivePacket && frame.size >= 12)
        {
            emit rfPacketReceived(device_transport::byte_codec::read64(frame.data, 1), device_transport::byte_codec::read16(frame.data, 9), QByteArray(reinterpret_cast<const char *>(frame.data + 12), static_cast<int>(frame.size - 12)));
        }
        else if (frameType == device_transport::api_frame::type::atCommandResponse && frame.size >= 5)
        {
            emit atCommandResponseReceived(frame.data[1], device_transport::byte_codec::read16(frame.data, 2), frame.data[4], QByteArray(reinterpret_cast<const char *>(frame.data + 5), static_cast<int>(frame.size - 5)));
        }
        else if (frameType == device_transport::api_frame::type::modemStatus && frame.size >= 2)
        {
            emit modemStatusReceived(frame.data[1]);
        }
        else if (frameType == device_transport::api_frame::type::transmitStatus && frame.size >= 7)
        {
            emit transmitStatusReceived(frame.data[1], device_transport::byte_codec::read16(frame.data, 2), frame.data[4], frame.data[5], frame.data[6]);
        }
        else if (frameType == device_transport::api_frame::type::remoteAtCommandResponse && frame.size >= 15)
        {
            emit remoteAtCommandResponseReceived(frame.data[1], device_transport::byte_codec::read64(frame.data, 2), device_transport::byte_codec::read16(frame.data, 10), device_transport::byte_codec::read16(frame.data, 12), frame.data[14], QByteArray(reinterpret_cast<const char *>(frame.data + 15), static_cast<int>(frame.size - 15)));
        }

        _escapeNext = false;
        _insideFrame = false;
    }

    quint8 QXBee::_allocateFrameId(const bool requestReply)
    {
        if (!requestReply)
        {
            return 0;
        }

        if (_nextFrameId == 0)
        {
            _nextFrameId = 1;
        }
        return _nextFrameId++;
    }

    quint8 QXBee::_calcChecksum(const QByteArray &frameData) const
    {
        return static_cast<quint8>(xbee_protocol::calculateChecksum(reinterpret_cast<const uint8_t *>(frameData.constData()), static_cast<size_t>(frameData.size())));
    }

    void QXBee::_writeEscaped(QByteArray &out, const quint8 byte) const
    {
        if (_apiMode == ApiMode::Api2 && (byte == device_transport::api_frame::startDelimiter || byte == device_transport::api_frame::escape || byte == device_transport::api_frame::xon || byte == device_transport::api_frame::xoff))
        {
            out.append(static_cast<char>(device_transport::api_frame::escape));
            out.append(static_cast<char>(byte ^ 0x20));
            return;
        }

        out.append(static_cast<char>(byte));
    }

    bool QXBee::_writeFrame(const QByteArray &frameData)
    {
        QByteArray encoded;
        encoded.append(static_cast<char>(device_transport::api_frame::startDelimiter));
        _writeEscaped(encoded, static_cast<quint8>((frameData.size() >> 8) & 0xFF));
        _writeEscaped(encoded, static_cast<quint8>(frameData.size() & 0xFF));
        for (const unsigned char byte : frameData)
        {
            _writeEscaped(encoded, byte);
        }
        _writeEscaped(encoded, _calcChecksum(frameData));
        return _port.write(encoded) == encoded.size();
    }

    quint8 QXBee::transmitRequest(const quint8 assignId, const quint64 dest64, const quint16 dest16, const QByteArray &rfPayload, const quint8 broadcastRadius, const quint8 options)
    {
        QByteArray frameData;
        frameData.append(static_cast<char>(device_transport::api_frame::type::transmitRequest));
        frameData.append(static_cast<char>(_allocateFrameId(assignId != 0)));
        for (int i = 7; i >= 0; --i)
        {
            frameData.append(static_cast<char>(dest64 >> (8 * i)));
        }
        frameData.append(static_cast<char>(dest16 >> 8));
        frameData.append(static_cast<char>(dest16));
        frameData.append(static_cast<char>(broadcastRadius));
        frameData.append(static_cast<char>(options));
        frameData.append(rfPayload);
        return _writeFrame(frameData) ? static_cast<quint8>(frameData[1]) : 0xFF;
    }

    quint8 QXBee::atCommandRequest(const quint16 atCommand, const QByteArray &parameter, const bool requestReply)
    {
        QByteArray frameData;
        frameData.append(static_cast<char>(device_transport::api_frame::type::atCommandRequest));
        frameData.append(static_cast<char>(_allocateFrameId(requestReply)));
        frameData.append(static_cast<char>(atCommand >> 8));
        frameData.append(static_cast<char>(atCommand));
        frameData.append(parameter);
        return _writeFrame(frameData) ? static_cast<quint8>(frameData[1]) : 0xFF;
    }

    quint8 QXBee::remoteAtCommandRequest(const quint64 dest64, const quint16 dest16, const quint16 atCommand, const QByteArray &parameter, const quint8 remoteCommandOptions, const bool requestReply)
    {
        QByteArray frameData;
        frameData.append(static_cast<char>(device_transport::api_frame::type::remoteAtCommandRequest));
        frameData.append(static_cast<char>(_allocateFrameId(requestReply)));
        for (int i = 7; i >= 0; --i)
        {
            frameData.append(static_cast<char>(dest64 >> (8 * i)));
        }
        frameData.append(static_cast<char>(dest16 >> 8));
        frameData.append(static_cast<char>(dest16));
        frameData.append(static_cast<char>(remoteCommandOptions));
        frameData.append(static_cast<char>(atCommand >> 8));
        frameData.append(static_cast<char>(atCommand));
        frameData.append(parameter);
        return _writeFrame(frameData) ? static_cast<quint8>(frameData[1]) : 0xFF;
    }
}
