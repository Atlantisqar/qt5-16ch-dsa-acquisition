#pragma once

#include <QVector>
#include <QWidget>

class WaveformView : public QWidget {
    Q_OBJECT

public:
    explicit WaveformView(QWidget* parent = nullptr);

    void setChannelIndex(int channelIndex);
    int channelIndex() const;
    void setSamples(const QVector<double>& samples);
    void setShowAxes(bool showAxes);
    bool showAxes() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_channelIndex = 0;
    QVector<double> m_samples;
    bool m_showAxes = false;
};
