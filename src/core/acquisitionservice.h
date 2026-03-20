#pragma once

#include "core/dsa16chdeviceservice.h"
#include "core/multichanneldatastore.h"

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QTimer>
#include <QVector>
#include <array>
#include <memory>
#include <mutex>

class AcquisitionService : public QObject {
    Q_OBJECT

public:
    explicit AcquisitionService(Dsa16ChDeviceService* deviceService,
                                MultiChannelDataStore* dataStore,
                                QObject* parent = nullptr);
    ~AcquisitionService() override;

    void setAcquisitionSettings(const dsa::DsaAcquisitionSettings& settings);
    void setDataDirectory(const QString& dataDirectory);
    void setMockModeEnabled(bool enabled);

    bool startAcquisition();
    void stopAcquisition();
    bool isRunning() const;
    bool isMockModeEnabled() const;
    QString lastError() const;

signals:
    void runningChanged(bool running);
    void errorOccurred(const QString& message);
    void bufferPointCountUpdated(unsigned int pointsPerChannel);
    void overflowDetected(bool overflow);
    void frameStored(quint64 frameIndex, unsigned int pointCountPerChannel);

private slots:
    void onPollTimeout();

private:
    bool startRealAcquisition();
    bool startMockAcquisition();

    unsigned int configuredReadChunk() const;
    bool computePointsToRead(unsigned int bufferPointCount, unsigned int& pointsToRead) const;
    void processReadFrame(unsigned int pointsToRead,
                          std::array<QVector<double>, dsa::kChannelCount>&& samples);
    void generateMockAndStore(unsigned int pointCount);
    bool shouldWriteToDisk() const;

    bool ensureOutputFilesReady();
    bool ensureSessionBinaryFileReady(bool forceCreateNew);
    bool rotateSessionBinaryFile();
    bool isContinuousModeForFileSplit() const;
    qint64 estimateFrameBytes(const AcquisitionFrame& frame) const;
    bool flushSessionBinaryBuffer();
    void appendFrameToSessionBinaryBuffer(const AcquisitionFrame& frame);
    void writeFrameToDisk(const AcquisitionFrame& frame);
    void writeLegacyChannelFiles(const AcquisitionFrame& frame);
    void closeOutputFiles();
    bool setError(const QString& message);

private:
    Dsa16ChDeviceService* m_deviceService = nullptr;
    MultiChannelDataStore* m_dataStore = nullptr;

    dsa::DsaAcquisitionSettings m_settings;
    QString m_dataDirectory;
    QString m_lastError;
    mutable std::mutex m_errorMutex;

    bool m_running = false;
    bool m_mockModeEnabled = false;
    bool m_mockRunning = false;
    bool m_lastOverflow = false;
    unsigned int m_lastBufferPointCount = 0;
    QTimer m_pollTimer;

    std::array<std::unique_ptr<QFile>, dsa::kChannelCount> m_channelFiles;
    std::unique_ptr<QFile> m_sessionBinaryFile;
    QString m_sessionBinaryFilePath;
    QString m_sessionBinaryBaseName;
    QByteArray m_sessionBinaryWriteBuffer;
    quint32 m_sessionBinaryFileIndex = 0;
    qint64 m_sessionStartMs = 0;
    double m_mockPhase = 0.0;
    qint64 m_lastBufferUiUpdateMs = 0;
    qint64 m_lastOverflowQueryMs = 0;
    unsigned int m_lastReportedBufferPointCount = 0;
};
