#pragma once

#include "core/dsa16chdllloader.h"
#include "core/dsa16chtypes.h"

#include <QObject>
#include <QVector>
#include <array>

class Dsa16ChDeviceService : public QObject {
    Q_OBJECT

public:
    explicit Dsa16ChDeviceService(Dsa16ChDllLoader* loader, QObject* parent = nullptr);
    ~Dsa16ChDeviceService() override;

    bool initializeSdk(const QString& preferredPath = QString());
    bool sdkReady() const;
    QString sdkStatusMessage() const;

    bool openDevice();
    void closeDevice();
    bool isDeviceOpen() const;

    bool setAcquisitionSettings(const dsa::DsaAcquisitionSettings& settings);
    dsa::DsaAcquisitionSettings currentSettings() const;
    dsa::DsaDioSettings currentDioSettings() const;

    bool setSampleRate(unsigned int sampleRateSel);
    bool setSampleMode(unsigned int sampleMode);
    bool setTriggerSource(unsigned int trigSrc);
    bool setExternalTriggerEdge(unsigned int edge);
    bool setClockBase(unsigned int clockBase);
    bool setFixedPointCount(unsigned int pointCountPerCh);

    bool startAcquisition();
    bool stopAcquisition();
    bool queryBufferPointCount(unsigned int& pointCountPerCh);
    bool readData(unsigned int pointNum2Read, std::array<QVector<double>, dsa::kChannelCount>& outSamples);
    bool queryOverflow(bool& overflow);

    bool setDioDirection(unsigned int groupIndex, unsigned int direction);
    bool writeDo(unsigned int groupIndex, unsigned char doData);
    bool readDi(unsigned int groupIndex, unsigned char& diData);

    QString lastError() const;
    void resetCachedState();
    void shutdown();

signals:
    void sdkStateChanged(bool ready, const QString& message);
    void deviceOpenStateChanged(bool opened);

private:
    bool ensureSdkReady(const QString& operation);
    bool ensureDeviceOpened(const QString& operation);
    bool setError(const QString& message);

private:
    Dsa16ChDllLoader* m_loader = nullptr;
    bool m_deviceOpen = false;
    QString m_lastError;
    dsa::DsaAcquisitionSettings m_settings;
    dsa::DsaDioSettings m_dioSettings;
};
