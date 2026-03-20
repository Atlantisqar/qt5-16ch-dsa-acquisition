#pragma once

#include "widgets/waveformview.h"

#include <QGridLayout>
#include <QMap>
#include <QVector>
#include <QWidget>

class WaveformGridWidget : public QWidget {
    Q_OBJECT

public:
    explicit WaveformGridWidget(QWidget* parent = nullptr);

    void setActiveChannels(const QVector<int>& channels);
    QVector<int> activeChannels() const;
    void clearChannelData();
    void updateChannelData(const QHash<int, QVector<double>>& channelData);

private:
    void rebuildLayout();
    void clearLayout();

private:
    QGridLayout* m_layout = nullptr;
    QVector<int> m_activeChannels;
    QMap<int, WaveformView*> m_views;
    QWidget* m_placeholder = nullptr;
};
