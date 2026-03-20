#pragma once

#include "core/dsa16chtypes.h"
#include "core/multichanneldatastore.h"
#include "widgets/channelselectorwidget.h"
#include "widgets/waveformgridwidget.h"

#include <QLabel>
#include <QProgressBar>
#include <QWidget>

class AcquisitionPage : public QWidget {
    Q_OBJECT

public:
    explicit AcquisitionPage(MultiChannelDataStore* dataStore, QWidget* parent = nullptr);

    void setDeviceOpened(bool opened);
    void setAcquisitionRunning(bool running);
    void setPlottingRunning(bool running);
    void setOverflow(bool overflow);
    void setBufferPointCount(unsigned int pointsPerChannel);
    void refreshWaveforms(int maxPoints = 2400);
    void clearWaveforms();
    void setSelectedChannels(const QVector<int>& channels);

    QVector<int> selectedChannels() const;

signals:
    void channelsSelectionChanged(const QVector<int>& channels);

private:
    void updateBufferUsageStyle(unsigned int pointsPerChannel);

private:
    MultiChannelDataStore* m_dataStore = nullptr;
    ChannelSelectorWidget* m_selectorWidget = nullptr;
    WaveformGridWidget* m_waveformGrid = nullptr;
    QLabel* m_deviceStatusLabel = nullptr;
    QLabel* m_acquisitionStatusLabel = nullptr;
    QLabel* m_plotStatusLabel = nullptr;
    QLabel* m_overflowLabel = nullptr;
    QProgressBar* m_bufferPointBar = nullptr;
    QLabel* m_bufferPointValueLabel = nullptr;
};
