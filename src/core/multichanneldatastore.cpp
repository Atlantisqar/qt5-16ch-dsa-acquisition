#include "core/multichanneldatastore.h"

#include <QWriteLocker>
#include <QReadLocker>

MultiChannelDataStore::MultiChannelDataStore(int capacityPerChannel, QObject* parent)
    : QObject(parent), m_capacityPerChannel(capacityPerChannel) {
    for (ChannelSampleBuffer& channel : m_channels) {
        channel.buffer.setCapacity(m_capacityPerChannel);
        channel.totalSamples = 0;
    }
}

void MultiChannelDataStore::clear() {
    QWriteLocker locker(&m_lock);
    for (ChannelSampleBuffer& channel : m_channels) {
        channel.buffer.clear();
        channel.totalSamples = 0;
    }
    m_frameCounter = 0;
}

quint64 MultiChannelDataStore::appendFrame(const AcquisitionFrame& frame) {
    QWriteLocker locker(&m_lock);
    for (int i = 0; i < dsa::kChannelCount; ++i) {
        m_channels[i].buffer.append(frame.channels[i]);
        m_channels[i].totalSamples += static_cast<quint64>(frame.channels[i].size());
    }
    ++m_frameCounter;
    const quint64 frameIndex = m_frameCounter;
    locker.unlock();
    emit dataAppended(frameIndex, frame.pointCountPerChannel);
    return frameIndex;
}

quint64 MultiChannelDataStore::appendChannelData(int channelIndex, const QVector<double>& samples) {
    if (channelIndex < 0 || channelIndex >= dsa::kChannelCount) {
        return 0;
    }
    QWriteLocker locker(&m_lock);
    m_channels[channelIndex].buffer.append(samples);
    m_channels[channelIndex].totalSamples += static_cast<quint64>(samples.size());
    return m_channels[channelIndex].totalSamples;
}

QVector<double> MultiChannelDataStore::latestSamples(int channelIndex, int maxPoints) const {
    if (channelIndex < 0 || channelIndex >= dsa::kChannelCount || maxPoints <= 0) {
        return {};
    }
    QReadLocker locker(&m_lock);
    return m_channels[channelIndex].buffer.latest(maxPoints);
}

QHash<int, QVector<double>> MultiChannelDataStore::latestSamples(const QVector<int>& channelIndexes,
                                                                 int maxPoints) const {
    QHash<int, QVector<double>> result;
    if (maxPoints <= 0) {
        return result;
    }

    QReadLocker locker(&m_lock);
    for (int channelIndex : channelIndexes) {
        if (channelIndex < 0 || channelIndex >= dsa::kChannelCount) {
            continue;
        }
        result.insert(channelIndex, m_channels[channelIndex].buffer.latest(maxPoints));
    }
    return result;
}

int MultiChannelDataStore::bufferedPointCount(int channelIndex) const {
    if (channelIndex < 0 || channelIndex >= dsa::kChannelCount) {
        return 0;
    }
    QReadLocker locker(&m_lock);
    return m_channels[channelIndex].buffer.size();
}

quint64 MultiChannelDataStore::totalSamples(int channelIndex) const {
    if (channelIndex < 0 || channelIndex >= dsa::kChannelCount) {
        return 0;
    }
    QReadLocker locker(&m_lock);
    return m_channels[channelIndex].totalSamples;
}

int MultiChannelDataStore::capacityPerChannel() const {
    return m_capacityPerChannel;
}
