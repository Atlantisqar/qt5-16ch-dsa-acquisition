#pragma once

#include "core/circularbuffer.h"
#include "core/dsa16chtypes.h"

#include <QObject>
#include <QHash>
#include <QReadWriteLock>
#include <QVector>
#include <array>

struct AcquisitionFrame {
    qint64 timestampMs = 0;
    unsigned int pointCountPerChannel = 0;
    std::array<QVector<double>, dsa::kChannelCount> channels;
};

struct ChannelSampleBuffer {
    CircularBuffer buffer;
    quint64 totalSamples = 0;
};

class MultiChannelDataStore : public QObject {
    Q_OBJECT

public:
    explicit MultiChannelDataStore(int capacityPerChannel = 65536, QObject* parent = nullptr);

    void clear();
    quint64 appendFrame(const AcquisitionFrame& frame);
    quint64 appendChannelData(int channelIndex, const QVector<double>& samples);

    QVector<double> latestSamples(int channelIndex, int maxPoints) const;
    QHash<int, QVector<double>> latestSamples(const QVector<int>& channelIndexes, int maxPoints) const;
    int bufferedPointCount(int channelIndex) const;
    quint64 totalSamples(int channelIndex) const;
    int capacityPerChannel() const;

signals:
    void dataAppended(quint64 frameIndex, unsigned int pointCountPerChannel);

private:
    mutable QReadWriteLock m_lock;
    std::array<ChannelSampleBuffer, dsa::kChannelCount> m_channels;
    quint64 m_frameCounter = 0;
    int m_capacityPerChannel = 65536;
};
