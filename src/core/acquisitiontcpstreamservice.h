#pragma once

#include "core/dsa16chtypes.h"
#include "core/multichanneldatastore.h"

#include <QObject>

class QTcpSocket;

class AcquisitionTcpStreamService : public QObject {
    Q_OBJECT

public:
    explicit AcquisitionTcpStreamService(QObject* parent = nullptr);

    void setNetworkSettings(const dsa::DsaNetworkSettings& settings);
    dsa::DsaNetworkSettings networkSettings() const;

    bool startStreaming(bool force = false);
    void stopStreaming();
    bool sendFrame(const AcquisitionFrame& frame, const dsa::DsaAcquisitionSettings& acquisitionSettings);

    bool isEnabled() const;
    bool isConnected() const;
    QString statusMessage() const;

signals:
    void stateChanged(bool enabled, bool connected, const QString& message);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketErrorOccurred();

private:
    bool ensureConnectionStarted(bool force = false);
    QByteArray serializeFrame(const AcquisitionFrame& frame,
                              const dsa::DsaAcquisitionSettings& acquisitionSettings) const;
    void setState(bool enabled, bool connected, const QString& message);

private:
    dsa::DsaNetworkSettings m_settings;
    QTcpSocket* m_socket = nullptr;
    QString m_statusMessage;
    qint64 m_lastConnectAttemptMs = 0;
};
