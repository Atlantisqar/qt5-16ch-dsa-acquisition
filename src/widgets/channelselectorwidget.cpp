#include "widgets/channelselectorwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>

ChannelSelectorWidget::ChannelSelectorWidget(QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(10);

    QLabel* title = new QLabel(QStringLiteral("通道选择"), this);
    title->setObjectName("SectionTitle");
    title->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(title, 0, Qt::AlignHCenter);

    QVBoxLayout* actionLayout = new QVBoxLayout();
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(6);
    m_selectAllButton = new QPushButton(QStringLiteral("\u5168\u90e8\u9009\u62e9"), this);
    m_clearAllButton = new QPushButton(QStringLiteral("\u5168\u90e8\u53d6\u6d88"), this);
    m_selectAllButton->setObjectName("SelectorActionButton");
    m_clearAllButton->setObjectName("SelectorActionButton");
    m_selectAllButton->setMinimumHeight(30);
    m_clearAllButton->setMinimumHeight(30);
    connect(m_selectAllButton, &QPushButton::clicked, this, &ChannelSelectorWidget::onSelectAllClicked);
    connect(m_clearAllButton, &QPushButton::clicked, this, &ChannelSelectorWidget::onClearAllClicked);
    actionLayout->addWidget(m_selectAllButton);
    actionLayout->addWidget(m_clearAllButton);

    QWidget* listContainer = new QWidget(this);
    listContainer->setObjectName("ChannelListContainer");
    QVBoxLayout* listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(4);
    listLayout->setAlignment(Qt::AlignTop);

    for (int i = 0; i < 16; ++i) {
        QCheckBox* check = new QCheckBox(QString("CH%1").arg(i + 1), listContainer);
        check->setChecked(i < 4);
        check->setMinimumWidth(78);
        connect(check, &QCheckBox::toggled, this, &ChannelSelectorWidget::onAnyCheckboxToggled);
        m_channelChecks[i] = check;

        QWidget* row = new QWidget(listContainer);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(0);
        rowLayout->addStretch();
        rowLayout->addWidget(check, 0, Qt::AlignCenter);
        rowLayout->addStretch();
        listLayout->addWidget(row);
    }
    listLayout->addStretch(1);

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("ChannelScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->viewport()->setAutoFillBackground(false);
    scrollArea->setWidget(listContainer);
    rootLayout->addWidget(scrollArea, 1);
    rootLayout->addLayout(actionLayout);
}

QVector<int> ChannelSelectorWidget::selectedChannels() const {
    QVector<int> channels;
    for (int i = 0; i < 16; ++i) {
        if (m_channelChecks[i] != nullptr && m_channelChecks[i]->isChecked()) {
            channels.push_back(i);
        }
    }
    return channels;
}

void ChannelSelectorWidget::setSelectedChannels(const QVector<int>& channels) {
    const QSet<int> target = QSet<int>(channels.begin(), channels.end());
    for (int i = 0; i < 16; ++i) {
        if (m_channelChecks[i] == nullptr) {
            continue;
        }
        const bool checked = target.contains(i);
        if (m_channelChecks[i]->isChecked() == checked) {
            continue;
        }
        m_channelChecks[i]->blockSignals(true);
        m_channelChecks[i]->setChecked(checked);
        m_channelChecks[i]->blockSignals(false);
    }
    emit selectedChannelsChanged(selectedChannels());
}

void ChannelSelectorWidget::onAnyCheckboxToggled(bool /*checked*/) {
    emit selectedChannelsChanged(selectedChannels());
}

void ChannelSelectorWidget::onSelectAllClicked() {
    for (int i = 0; i < 16; ++i) {
        if (m_channelChecks[i] == nullptr) {
            continue;
        }
        m_channelChecks[i]->blockSignals(true);
        m_channelChecks[i]->setChecked(true);
        m_channelChecks[i]->blockSignals(false);
    }
    emit selectedChannelsChanged(selectedChannels());
}

void ChannelSelectorWidget::onClearAllClicked() {
    for (int i = 0; i < 16; ++i) {
        if (m_channelChecks[i] == nullptr) {
            continue;
        }
        m_channelChecks[i]->blockSignals(true);
        m_channelChecks[i]->setChecked(false);
        m_channelChecks[i]->blockSignals(false);
    }
    emit selectedChannelsChanged(selectedChannels());
}

