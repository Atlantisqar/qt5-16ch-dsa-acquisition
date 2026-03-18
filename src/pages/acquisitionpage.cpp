#include "pages/acquisitionpage.h"

#include "theme/themehelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QVBoxLayout>

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
    QHBoxLayout* statusLayout = new QHBoxLayout(statusCard);
    statusLayout->setContentsMargins(16, 12, 16, 12);
    statusLayout->setSpacing(12);

    m_acquisitionStatusLabel = new QLabel(QStringLiteral("采集状态：已停止"), statusCard);
    m_plotStatusLabel = new QLabel(QStringLiteral("绘图状态：已停止"), statusCard);
    m_overflowLabel = new QLabel(QStringLiteral("溢出：否"), statusCard);
    m_bufferPointLabel = new QLabel(QStringLiteral("缓存点数/通道：0"), statusCard);

    QLabel* statusLabels[] = {
        m_acquisitionStatusLabel,
        m_plotStatusLabel,
        m_overflowLabel,
        m_bufferPointLabel};
    for (QLabel* label : statusLabels) {
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        statusLayout->addWidget(label, 1);
    }
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
}

void AcquisitionPage::setAcquisitionRunning(bool running) {
    m_acquisitionStatusLabel->setText(
        QStringLiteral("采集状态：%1").arg(running ? QStringLiteral("运行中") : QStringLiteral("已停止")));
}

void AcquisitionPage::setPlottingRunning(bool running) {
    m_plotStatusLabel->setText(
        QStringLiteral("绘图状态：%1").arg(running ? QStringLiteral("运行中") : QStringLiteral("已停止")));
}

void AcquisitionPage::setOverflow(bool overflow) {
    m_overflowLabel->setText(
        QStringLiteral("溢出：%1").arg(overflow ? QStringLiteral("是") : QStringLiteral("否")));
}

void AcquisitionPage::setBufferPointCount(unsigned int pointsPerChannel) {
    m_bufferPointLabel->setText(QStringLiteral("缓存点数/通道：%1").arg(pointsPerChannel));
}

void AcquisitionPage::refreshWaveforms(int maxPoints) {
    if (m_dataStore == nullptr) {
        return;
    }
    const QVector<int> channels = m_selectorWidget->selectedChannels();
    const QHash<int, QVector<double>> data = m_dataStore->latestSamples(channels, maxPoints);
    m_waveformGrid->updateChannelData(data);
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
