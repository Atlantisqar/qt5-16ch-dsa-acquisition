#include "pages/acquisitionpage.h"

#include "theme/themehelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString metricValueText(bool enabled) {
    return enabled ? QStringLiteral("\u8fd0\u884c\u4e2d") : QStringLiteral("\u5df2\u505c\u6b62");
}

QString modeStatusText(unsigned int mode) {
    return dsa::appModeToText(mode);
}

QString overflowText(bool overflow) {
    return overflow ? QStringLiteral("\u662f") : QStringLiteral("\u5426");
}

QString networkStatusText(bool enabled, bool connected, const QString&) {
    Q_UNUSED(enabled);
    return connected ? QStringLiteral("\u5df2\u8fde\u63a5") : QStringLiteral("\u672a\u8fde\u63a5");
}

void refreshWidgetStyle(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QFrame* createStatusMetricCard(QWidget* parent, QLabel** valueLabel) {
    QFrame* card = new QFrame(parent);
    card->setObjectName("StatusMetricCard");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(0);

    QLabel* contentLabel = new QLabel(card);
    contentLabel->setObjectName("MetricValue");
    contentLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    contentLabel->setWordWrap(true);

    layout->addWidget(contentLabel);

    if (valueLabel != nullptr) {
        *valueLabel = contentLabel;
    }
    return card;
}

}  // namespace

AcquisitionPage::AcquisitionPage(MultiChannelDataStore* dataStore, QWidget* parent)
    : QWidget(parent), m_dataStore(dataStore) {
    QHBoxLayout* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(14);

    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(14);

    QFrame* waveformCard = new QFrame(leftPanel);
    waveformCard->setObjectName("CardWidget");
    ThemeHelper::applyCardShadow(waveformCard);
    QVBoxLayout* waveformLayout = new QVBoxLayout(waveformCard);
    waveformLayout->setContentsMargins(16, 16, 16, 16);
    m_waveformGrid = new WaveformGridWidget(waveformCard);
    waveformLayout->addWidget(m_waveformGrid, 1);
    leftLayout->addWidget(waveformCard, 1);

    QFrame* statusCard = new QFrame(leftPanel);
    statusCard->setObjectName("CardWidget");
    ThemeHelper::applyCardShadow(statusCard);
    statusCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    statusCard->setFixedHeight(84);
    QHBoxLayout* statusLayout = new QHBoxLayout(statusCard);
    statusLayout->setContentsMargins(10, 10, 10, 10);
    statusLayout->setSpacing(8);

    statusLayout->addWidget(createStatusMetricCard(statusCard, &m_modeStatusLabel));
    statusLayout->addWidget(createStatusMetricCard(statusCard, &m_acquisitionStatusLabel));
    statusLayout->addWidget(createStatusMetricCard(statusCard, &m_plotStatusLabel));
    statusLayout->addWidget(createStatusMetricCard(statusCard, &m_networkStatusLabel));
    statusLayout->addWidget(createStatusMetricCard(statusCard, &m_overflowLabel));

    m_bufferCard = new QFrame(statusCard);
    m_bufferCard->setObjectName("StatusMetricCard");
    m_bufferCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout* bufferLayout = new QVBoxLayout(m_bufferCard);
    bufferLayout->setContentsMargins(12, 10, 12, 10);
    bufferLayout->setSpacing(4);

    m_bufferPointBar = new QProgressBar(m_bufferCard);
    m_bufferPointBar->setObjectName("BufferProgressBar");
    m_bufferPointBar->setRange(0, static_cast<int>(dsa::kMaxPointPerChannel));
    m_bufferPointBar->setTextVisible(false);
    m_bufferPointBar->setFixedHeight(10);

    m_bufferPointValueLabel = new QLabel(m_bufferCard);
    m_bufferPointValueLabel->setObjectName("MetricValue");
    m_bufferPointValueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    bufferLayout->addWidget(m_bufferPointBar);
    bufferLayout->addWidget(m_bufferPointValueLabel);
    statusLayout->addWidget(m_bufferCard, 2);

    leftLayout->addWidget(statusCard, 0);

    QFrame* selectorCard = new QFrame(this);
    selectorCard->setObjectName("CardWidget");
    ThemeHelper::applyCardShadow(selectorCard);
    selectorCard->setMinimumWidth(114);
    selectorCard->setMaximumWidth(140);
    QVBoxLayout* selectorLayout = new QVBoxLayout(selectorCard);
    selectorLayout->setContentsMargins(16, 16, 16, 16);
    m_selectorWidget = new ChannelSelectorWidget(selectorCard);
    selectorLayout->addWidget(m_selectorWidget);

    rootLayout->addWidget(leftPanel, 1);
    rootLayout->addWidget(selectorCard, 0);

    connect(m_selectorWidget,
            &ChannelSelectorWidget::selectedChannelsChanged,
            m_waveformGrid,
            &WaveformGridWidget::setActiveChannels);
    connect(m_selectorWidget,
            &ChannelSelectorWidget::selectedChannelsChanged,
            this,
            &AcquisitionPage::channelsSelectionChanged);

    m_waveformGrid->setActiveChannels(m_selectorWidget->selectedChannels());

    setAppMode(static_cast<unsigned int>(dsa::AppMode::Sender));
    setDeviceOpened(false);
    setAcquisitionRunning(false);
    setPlottingRunning(false);
    setNetworkState(false, false);
    setOverflow(false);
    setBufferPointCount(0);
}

void AcquisitionPage::setAppMode(unsigned int appMode) {
    m_appMode = appMode;
    m_modeStatusLabel->setText(QStringLiteral("\u6a21\u5f0f\uff1a%1").arg(modeStatusText(appMode)));
    if (m_bufferCard != nullptr) {
        const bool isReceiverMode = appMode == static_cast<unsigned int>(dsa::AppMode::Receiver);
        m_bufferCard->setVisible(!isReceiverMode);
    }
}

void AcquisitionPage::setDeviceOpened(bool opened) {
    Q_UNUSED(opened);
}

void AcquisitionPage::setAcquisitionRunning(bool running) {
    const QString prefix = (m_appMode == static_cast<unsigned int>(dsa::AppMode::Receiver))
                               ? QStringLiteral("\u63a5\u6536")
                               : QStringLiteral("\u91c7\u96c6");
    m_acquisitionStatusLabel->setText(
        QStringLiteral("%1\uff1a%2").arg(prefix).arg(metricValueText(running)));
}

void AcquisitionPage::setPlottingRunning(bool running) {
    m_plotStatusLabel->setText(
        QStringLiteral("\u7ed8\u56fe\uff1a%1").arg(metricValueText(running)));
}

void AcquisitionPage::setNetworkState(bool enabled, bool connected, const QString& message) {
    m_networkStatusLabel->setText(
        QStringLiteral("\u7f51\u7edc\uff1a%1").arg(networkStatusText(enabled, connected, message)));
    m_networkStatusLabel->setProperty("alert", enabled && !connected);
    refreshWidgetStyle(m_networkStatusLabel);
}

void AcquisitionPage::setOverflow(bool overflow) {
    m_overflowLabel->setText(
        QStringLiteral("\u6ea2\u51fa\uff1a%1").arg(overflowText(overflow)));
    m_overflowLabel->setProperty("alert", overflow);
    refreshWidgetStyle(m_overflowLabel);
}

void AcquisitionPage::setBufferPointCount(unsigned int pointsPerChannel) {
    const unsigned int clampedValue = qMin(pointsPerChannel, dsa::kMaxPointPerChannel);
    m_bufferPointBar->setValue(static_cast<int>(clampedValue));
    m_bufferPointValueLabel->setText(
        QStringLiteral("\u7f13\u5b58\uff1a%1 / %2").arg(clampedValue).arg(dsa::kMaxPointPerChannel));
    updateBufferUsageStyle(clampedValue);
}

void AcquisitionPage::refreshWaveforms(int maxPoints) {
    if (m_dataStore == nullptr) {
        return;
    }
    const QVector<int> channels = m_selectorWidget->selectedChannels();
    const QHash<int, QVector<double>> data = m_dataStore->latestSamples(channels, maxPoints);
    m_waveformGrid->updateChannelData(data);
}

void AcquisitionPage::clearWaveforms() {
    if (m_waveformGrid == nullptr) {
        return;
    }
    m_waveformGrid->clearChannelData();
}

void AcquisitionPage::setSelectedChannels(const QVector<int>& channels) {
    if (m_selectorWidget == nullptr) {
        return;
    }
    m_selectorWidget->setSelectedChannels(channels);
}

QVector<int> AcquisitionPage::selectedChannels() const {
    return m_selectorWidget->selectedChannels();
}

void AcquisitionPage::updateBufferUsageStyle(unsigned int pointsPerChannel) {
    QString usageState = QStringLiteral("normal");
    if (pointsPerChannel >= (dsa::kMaxPointPerChannel * 7U) / 8U) {
        usageState = QStringLiteral("critical");
    } else if (pointsPerChannel >= (dsa::kMaxPointPerChannel * 3U) / 4U) {
        usageState = QStringLiteral("warning");
    }

    const QString currentBarState = m_bufferPointBar->property("usageState").toString();
    if (currentBarState == usageState) {
        return;
    }

    m_bufferPointBar->setProperty("usageState", usageState);
    m_bufferPointValueLabel->setProperty("usageState", usageState);
    refreshWidgetStyle(m_bufferPointBar);
    refreshWidgetStyle(m_bufferPointValueLabel);
}
