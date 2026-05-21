#pragma once

#include "xbee/frames.hpp"
#include "xbee/protocol.hpp"

#include <QByteArray>
#include <QObject>
#include <QSerialPort>
#include <QtGlobal>

namespace device_transport
{
class QXBee : public QObject
{
    Q_OBJECT

public:
    enum class ApiMode : quint8
    {
        Api1 = 1,
        Api2 = 2
    };

    explicit QXBee(QObject *parent = nullptr);

    bool open(const QString &portName, int baudRate, ApiMode mode = ApiMode::Api2);
    void close();
    bool isOpen() const;

    ApiMode apiMode() const;
    void setApiMode(ApiMode mode);

    quint8 transmitRequest(quint8 assignId, quint64 dest64, quint16 dest16, const QByteArray &rfPayload, quint8 broadcastRadius = 0x00, quint8 options = 0x00);
    quint8 atCommandRequest(quint16 atCommand, const QByteArray &parameter = QByteArray(), bool requestReply = true);
    quint8 remoteAtCommandRequest(quint64 dest64, quint16 dest16, quint16 atCommand, const QByteArray &parameter = QByteArray(), quint8 remoteCommandOptions = 0x02, bool requestReply = true);

signals:
    void rfPacketReceived(quint64 src64, quint16 src16, QByteArray payload);
    void atCommandResponseReceived(quint8 frameId, quint16 atCommand, quint8 status, QByteArray value);
    void remoteAtCommandResponseReceived(quint8 frameId, quint64 src64, quint16 src16, quint16 atCommand, quint8 status, QByteArray value);
    void transmitStatusReceived(quint8 frameId, quint16 dest16, quint8 retryCount, quint8 deliveryStatus, quint8 discoveryStatus);
    void modemStatusReceived(quint8 status);
    void errorOccurred(const QString &message);

private slots:
    void _onReadyRead();

private:
    QSerialPort _port;
    ApiMode _apiMode{ApiMode::Api2};
    bool _insideFrame{};
    bool _escapeNext{};
    xbee_protocol::FrameParser<512> _parser;
    quint8 _nextFrameId{1};

    void _processByte(quint8 byte);
    quint8 _allocateFrameId(bool requestReply);
    quint8 _calcChecksum(const QByteArray &frameData) const;
    void _writeEscaped(QByteArray &out, quint8 byte) const;
    bool _writeFrame(const QByteArray &frameData);
};
}
