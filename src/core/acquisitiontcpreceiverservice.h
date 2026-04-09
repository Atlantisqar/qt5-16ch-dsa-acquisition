#pragma once

#include "core/dsa16chtypes.h"
#include "core/multichanneldatastore.h"

#include <QObject>
#include <memory>

class QTcpServer;
class QTcpSocket;
class QFile;

class AcquisitionTcpReceiverService : public QObject {
    Q_OBJECT

public:
    explicit AcquisitionTcpReceiverService(MultiChannelDataStore* dataStore, QObject* parent = nullptr);
    ~AcquisitionTcpReceiverService() override;

    void setReceiverSettings(const dsa::DsaReceiverSettings& settings);
    void setDataDirectory(const QString& dataDirectory);

    bool startReceiving();
    void stopReceiving();

    bool isRunning() const;
    bool isClientConnected() const;
    QString statusMessage() const;

signals:
    void runningChanged(bool running);
    void networkStateChanged(bool enabled, bool connected, const QString& message);
    void bufferPointCountUpdated(unsigned int pointsPerChannel);
    void errorOccurred(const QString& message);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onClientReadyRead();
    void onClientErrorOccurred();

private:
    bool ensureOutputFileReady();
    bool openSessionFile();
    bool writeFrameToDisk(const AcquisitionFrame& frame);
    bool processPendingFrames();
    bool setError(const QString& message);
    void setState(bool enabled, bool connected, const QString& message);

private:
    MultiChannelDataStore* m_dataStore = nullptr;
    dsa::DsaReceiverSettings m_settings;
    QString m_dataDirectory;
    QTcpServer* m_server = nullptr;
    QTcpSocket* m_clientSocket = nullptr;
    QByteArray m_readBuffer;
    std::unique_ptr<QFile> m_sessionFile;
    QString m_statusMessage;
    bool m_running = false;
};
