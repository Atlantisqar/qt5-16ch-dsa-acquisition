#include "widgets/waveformview.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <QtMath>

WaveformView::WaveformView(QWidget* parent)
    : QWidget(parent) {
    // Keep each tile shrinkable so many channels won't push the whole window out of screen.
    setMinimumSize(80, 60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void WaveformView::setChannelIndex(int channelIndex) {
    m_channelIndex = channelIndex;
    update();
}

int WaveformView::channelIndex() const {
    return m_channelIndex;
}

void WaveformView::setSamples(const QVector<double>& samples) {
    m_samples = samples;
    update();
}

void WaveformView::setShowAxes(bool showAxes) {
    if (m_showAxes == showAxes) {
        return;
    }
    m_showAxes = showAxes;
    update();
}

bool WaveformView::showAxes() const {
    return m_showAxes;
}

void WaveformView::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF fullRect = rect().adjusted(2, 2, -2, -2);
    painter.setPen(QPen(QColor(255, 255, 255, 200), 1.0));
    painter.setBrush(QColor(255, 255, 255, 150));
    painter.drawRoundedRect(fullRect, 5.0, 5.0);

    const qreal leftInset = m_showAxes ? 50.0 : 12.0;
    const qreal topInset = 24.0;
    const qreal rightInset = 12.0;
    const qreal bottomInset = m_showAxes ? 32.0 : 12.0;
    const QRectF plotRect = fullRect.adjusted(leftInset, topInset, -rightInset, -bottomInset);
    painter.setPen(QPen(QColor("#E8ECF3"), 1));
    const int gridX = 8;
    const int gridY = 6;
    for (int i = 0; i <= gridX; ++i) {
        const qreal x = plotRect.left() + plotRect.width() * static_cast<qreal>(i) / gridX;
        painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
    }
    for (int i = 0; i <= gridY; ++i) {
        const qreal y = plotRect.top() + plotRect.height() * static_cast<qreal>(i) / gridY;
        painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
    }

    painter.setPen(QColor("#1F2742"));
    painter.setFont(QFont("Microsoft YaHei UI", 9, QFont::DemiBold));
    painter.drawText(QRectF(fullRect.left() + 10, fullRect.top() + 4, 120, 18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString("CH%1").arg(m_channelIndex + 1));

    painter.setPen(QColor("#7B7F87"));
    painter.setFont(QFont("Microsoft YaHei UI", 8));
    painter.drawText(QRectF(fullRect.right() - 90, fullRect.top() + 4, 80, 18),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString("%1 pts").arg(m_samples.size()));

    const int sampleCount = m_samples.size();
    double minVal = -1.0;
    double maxVal = 1.0;
    if (sampleCount >= 1) {
        minVal = m_samples.front();
        maxVal = m_samples.front();
        for (double v : m_samples) {
            minVal = qMin(minVal, v);
            maxVal = qMax(maxVal, v);
        }
        if (qFuzzyCompare(minVal, maxVal)) {
            minVal -= 1.0;
            maxVal += 1.0;
        }
    }

    if (m_showAxes) {
        painter.setPen(QPen(QColor("#AAB3C2"), 1));
        painter.drawLine(QPointF(plotRect.left(), plotRect.top()), QPointF(plotRect.left(), plotRect.bottom()));
        painter.drawLine(QPointF(plotRect.left(), plotRect.bottom()), QPointF(plotRect.right(), plotRect.bottom()));

        painter.setPen(QColor("#6F7785"));
        painter.setFont(QFont("Microsoft YaHei UI", 7));

        const int yTicks = 4;
        for (int i = 0; i <= yTicks; ++i) {
            const qreal y = plotRect.top() + plotRect.height() * static_cast<qreal>(i) / yTicks;
            const double value = maxVal - (maxVal - minVal) * static_cast<double>(i) / yTicks;
            painter.drawLine(QPointF(plotRect.left() - 4, y), QPointF(plotRect.left(), y));
            painter.drawText(QRectF(fullRect.left() + 2, y - 8, plotRect.left() - fullRect.left() - 8, 16),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(value, 'f', 2));
        }

        const int xTicks = 4;
        const int axisSampleCount = qMax(1, sampleCount);
        for (int i = 0; i <= xTicks; ++i) {
            const qreal ratio = static_cast<qreal>(i) / xTicks;
            const qreal x = plotRect.left() + plotRect.width() * ratio;
            const int xValue = qRound((axisSampleCount - 1) * ratio);
            painter.drawLine(QPointF(x, plotRect.bottom()), QPointF(x, plotRect.bottom() + 4));
            painter.drawText(QRectF(x - 24, plotRect.bottom() + 6, 48, 14),
                             Qt::AlignHCenter | Qt::AlignTop,
                             QString::number(xValue));
        }
    }

    if (sampleCount < 2) {
        painter.setPen(QColor("#9AA3B2"));
        painter.setFont(QFont("Microsoft YaHei UI", 8));
        painter.drawText(plotRect, Qt::AlignCenter, "No Data");
        return;
    }

    QPainterPath path;
    const int drawablePixels = std::max(1, static_cast<int>(plotRect.width()));
    const int maxRenderableSamples = std::max(2, drawablePixels * 2);
    const int step = std::max(1, sampleCount / maxRenderableSamples);

    for (int i = 0; i < sampleCount; i += step) {
        const qreal x = plotRect.left() + plotRect.width() * static_cast<qreal>(i) / (sampleCount - 1);
        const qreal normalized = (m_samples[i] - minVal) / (maxVal - minVal);
        const qreal y = plotRect.bottom() - normalized * plotRect.height();
        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    if (((sampleCount - 1) % step) != 0) {
        const qreal normalized = (m_samples.back() - minVal) / (maxVal - minVal);
        const qreal y = plotRect.bottom() - normalized * plotRect.height();
        path.lineTo(plotRect.right(), y);
    }

    painter.setClipRect(plotRect.adjusted(-1, -1, 1, 1));
    painter.setPen(QPen(QColor("#1F2742"), 1.0));
    painter.drawPath(path);
}
