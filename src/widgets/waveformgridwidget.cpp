#include "widgets/waveformgridwidget.h"

#include <QLabel>
#include <QSizePolicy>
#include <QtMath>

WaveformGridWidget::WaveformGridWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(12);

    QLabel* label = new QLabel("请选择至少 1 个通道", this);
    label->setAlignment(Qt::AlignCenter);
    label->setObjectName("HintLabel");
    m_placeholder = label;
    m_layout->addWidget(m_placeholder, 0, 0);
}

void WaveformGridWidget::setActiveChannels(const QVector<int>& channels) {
    m_activeChannels = channels;
    rebuildLayout();
}

QVector<int> WaveformGridWidget::activeChannels() const {
    return m_activeChannels;
}

void WaveformGridWidget::updateChannelData(const QHash<int, QVector<double>>& channelData) {
    for (auto it = channelData.constBegin(); it != channelData.constEnd(); ++it) {
        const int channel = it.key();
        WaveformView* view = m_views.value(channel, nullptr);
        if (view != nullptr) {
            view->setSamples(it.value());
        }
    }
}

void WaveformGridWidget::rebuildLayout() {
    clearLayout();
    m_views.clear();

    // Reset previous grid stretch settings. Otherwise, when channel count decreases
    // old row/column stretch factors may still affect the new layout.
    for (int i = 0; i < 16; ++i) {
        m_layout->setRowStretch(i, 0);
        m_layout->setColumnStretch(i, 0);
    }

    if (m_activeChannels.isEmpty()) {
        if (m_placeholder == nullptr) {
            QLabel* label = new QLabel("请选择至少 1 个通道", this);
            label->setAlignment(Qt::AlignCenter);
            label->setObjectName("HintLabel");
            m_placeholder = label;
        }
        m_layout->addWidget(m_placeholder, 0, 0);
        m_layout->setRowStretch(0, 1);
        m_layout->setColumnStretch(0, 1);
        m_layout->invalidate();
        updateGeometry();
        if (parentWidget() != nullptr) {
            parentWidget()->updateGeometry();
        }
        return;
    }

    if (m_placeholder != nullptr) {
        m_placeholder->setParent(nullptr);
        m_placeholder = nullptr;
    }

    const int count = m_activeChannels.size();
    int cols = qCeil(qSqrt(static_cast<double>(count)));
    if (count == 1) {
        cols = 1;
    } else if (count == 2) {
        // Force vertical stack for 2 channels: 1 column x 2 rows.
        cols = 1;
    }
    const int rows = qCeil(static_cast<double>(count) / cols);

    int index = 0;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            if (index >= count) {
                break;
            }
            const int channelIndex = m_activeChannels[index++];
            WaveformView* view = new WaveformView(this);
            view->setChannelIndex(channelIndex);
            view->setShowAxes(count <= 2);
            m_layout->addWidget(view, row, col);
            m_views.insert(channelIndex, view);
        }
    }

    for (int r = 0; r < rows; ++r) {
        m_layout->setRowStretch(r, 1);
    }
    for (int c = 0; c < cols; ++c) {
        m_layout->setColumnStretch(c, 1);
    }
    m_layout->invalidate();
    updateGeometry();
    if (parentWidget() != nullptr) {
        parentWidget()->updateGeometry();
    }
}

void WaveformGridWidget::clearLayout() {
    while (m_layout->count() > 0) {
        QLayoutItem* item = m_layout->takeAt(0);
        QWidget* widget = item->widget();
        if (widget != nullptr) {
            if (widget == m_placeholder) {
                m_placeholder = nullptr;
            }
            delete widget;
        }
        delete item;
    }
}
