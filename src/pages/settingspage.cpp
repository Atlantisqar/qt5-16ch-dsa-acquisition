#include "pages/settingspage.h"

#include "theme/themehelper.h"

#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

void configureFieldWidget(QWidget* widget, int minimumWidth = 300) {
    if (widget == nullptr) {
        return;
    }
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    widget->setMinimumWidth(minimumWidth);
}

QFrame* createCard(QWidget* parent, const QString& titleText, QVBoxLayout** outLayout) {
    auto* card = new QFrame(parent);
    card->setObjectName("CardWidget");
    ThemeHelper::applyCardShadow(card);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto* title = new QLabel(titleText, card);
    title->setObjectName("CardTitle");
    layout->addWidget(title);

    if (outLayout != nullptr) {
        *outLayout = layout;
    }
    return card;
}

QFormLayout* createFormLayout() {
    auto* form = new QFormLayout();
    form->setHorizontalSpacing(20);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return form;
}

}  // namespace

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rootLayout->addWidget(scrollArea);

    auto* contentWidget = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(14);
    scrollArea->setWidget(contentWidget);

    QVBoxLayout* modeLayout = nullptr;
    QFrame* modeCard = createCard(contentWidget, QStringLiteral("运行模式"), &modeLayout);
    auto* modeForm = createFormLayout();
    m_appModeCombo = new QComboBox(modeCard);
    m_appModeCombo->addItem(dsa::appModeToText(static_cast<unsigned int>(dsa::AppMode::Sender)),
                            static_cast<unsigned int>(dsa::AppMode::Sender));
    m_appModeCombo->addItem(dsa::appModeToText(static_cast<unsigned int>(dsa::AppMode::Receiver)),
                            static_cast<unsigned int>(dsa::AppMode::Receiver));
    configureFieldWidget(m_appModeCombo);
    modeForm->addRow(QStringLiteral("当前模式"), m_appModeCombo);
    modeLayout->addLayout(modeForm);
    connect(m_appModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPage::onAppModeChanged);
    contentLayout->addWidget(modeCard);

    QVBoxLayout* senderSettingsLayout = nullptr;
    m_senderSettingsCard = createCard(contentWidget, QStringLiteral("发送端采集参数"), &senderSettingsLayout);
    auto* senderForm = createFormLayout();

    m_sampleRateCombo = new QComboBox(m_senderSettingsCard);
    for (unsigned int i = 0; i <= static_cast<unsigned int>(dsa::SampleRateSel::Rate6_4K); ++i) {
        m_sampleRateCombo->addItem(dsa::sampleRateToText(i), i);
    }

    m_sampleModeCombo = new QComboBox(m_senderSettingsCard);
    for (unsigned int i = 0; i <= static_cast<unsigned int>(dsa::SampleMode::ContinuousFixedPoint); ++i) {
        m_sampleModeCombo->addItem(dsa::sampleModeToText(i), i);
    }

    m_triggerSourceCombo = new QComboBox(m_senderSettingsCard);
    for (unsigned int i = 0; i <= static_cast<unsigned int>(dsa::TriggerSource::ExternalSignal); ++i) {
        m_triggerSourceCombo->addItem(dsa::triggerSourceToText(i), i);
    }

    m_externalTriggerEdgeCombo = new QComboBox(m_senderSettingsCard);
    for (unsigned int i = 0; i <= static_cast<unsigned int>(dsa::ExternalTriggerEdge::Rising); ++i) {
        m_externalTriggerEdgeCombo->addItem(dsa::externalTriggerEdgeToText(i), i);
    }

    m_clockBaseCombo = new QComboBox(m_senderSettingsCard);
    for (unsigned int i = 0; i <= static_cast<unsigned int>(dsa::ClockBase::External); ++i) {
        m_clockBaseCombo->addItem(dsa::clockBaseToText(i), i);
    }

    m_fixedPointSpin = new QSpinBox(m_senderSettingsCard);
    m_fixedPointSpin->setRange(16, static_cast<int>(dsa::kMaxPointPerChannel));
    m_fixedPointSpin->setSingleStep(16);
    m_fixedPointSpin->setValue(4096);

    m_saveToDiskCheck = new QCheckBox(QStringLiteral("采集数据落盘保存到本地"), m_senderSettingsCard);
    m_saveToDiskCheck->setChecked(true);

    configureFieldWidget(m_sampleRateCombo);
    configureFieldWidget(m_sampleModeCombo);
    configureFieldWidget(m_triggerSourceCombo);
    configureFieldWidget(m_externalTriggerEdgeCombo);
    configureFieldWidget(m_clockBaseCombo);
    configureFieldWidget(m_fixedPointSpin);
    configureFieldWidget(m_saveToDiskCheck);

    senderForm->addRow(QStringLiteral("采样率"), m_sampleRateCombo);
    senderForm->addRow(QStringLiteral("采集模式"), m_sampleModeCombo);
    senderForm->addRow(QStringLiteral("触发源"), m_triggerSourceCombo);
    senderForm->addRow(QStringLiteral("外部触发沿"), m_externalTriggerEdgeCombo);
    senderForm->addRow(QStringLiteral("时钟基准"), m_clockBaseCombo);
    senderForm->addRow(QStringLiteral("定点每通道点数"), m_fixedPointSpin);
    senderForm->addRow(QStringLiteral("本地存储"), m_saveToDiskCheck);
    senderSettingsLayout->addLayout(senderForm);

    auto* senderButtonRow = new QHBoxLayout();
    auto* readButton = new QPushButton(QStringLiteral("读取当前配置"), m_senderSettingsCard);
    auto* applyButton = new QPushButton(QStringLiteral("应用配置"), m_senderSettingsCard);
    auto* saveButton = new QPushButton(QStringLiteral("保存到项目"), m_senderSettingsCard);
    connect(readButton, &QPushButton::clicked, this, &SettingsPage::onReadButtonClicked);
    connect(applyButton, &QPushButton::clicked, this, &SettingsPage::onApplyButtonClicked);
    connect(saveButton, &QPushButton::clicked, this, &SettingsPage::onSaveProjectButtonClicked);
    senderButtonRow->addWidget(readButton);
    senderButtonRow->addWidget(applyButton);
    senderButtonRow->addWidget(saveButton);
    senderButtonRow->addStretch();
    senderSettingsLayout->addLayout(senderButtonRow);
    contentLayout->addWidget(m_senderSettingsCard);

    QVBoxLayout* senderNetworkLayout = nullptr;
    m_senderNetworkCard = createCard(contentWidget, QStringLiteral("发送端网络配置"), &senderNetworkLayout);
    auto* senderNetworkForm = createFormLayout();
    m_enableNetworkCheck = new QCheckBox(QStringLiteral("启用 TCP 数据发送"), m_senderNetworkCard);
    m_remoteHostEdit = new QLineEdit(m_senderNetworkCard);
    m_remoteHostEdit->setPlaceholderText(QStringLiteral("192.168.1.100"));
    m_remotePortSpin = new QSpinBox(m_senderNetworkCard);
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(9000);
    configureFieldWidget(m_enableNetworkCheck);
    configureFieldWidget(m_remoteHostEdit);
    configureFieldWidget(m_remotePortSpin);
    senderNetworkForm->addRow(QStringLiteral("发送开关"), m_enableNetworkCheck);
    senderNetworkForm->addRow(QStringLiteral("目标 IP / Host"), m_remoteHostEdit);
    senderNetworkForm->addRow(QStringLiteral("目标端口"), m_remotePortSpin);
    senderNetworkLayout->addLayout(senderNetworkForm);
    auto* connectNetworkRow = new QHBoxLayout();
    auto* connectNetworkButton = new QPushButton(QStringLiteral("连接"), m_senderNetworkCard);
    connect(connectNetworkButton, &QPushButton::clicked, this, &SettingsPage::onConnectNetworkButtonClicked);
    connectNetworkRow->addWidget(connectNetworkButton);
    connectNetworkRow->addStretch();
    senderNetworkLayout->addLayout(connectNetworkRow);
    contentLayout->addWidget(m_senderNetworkCard);

    QVBoxLayout* receiverLayout = nullptr;
    m_receiverCard = createCard(contentWidget, QStringLiteral("接收端监听配置"), &receiverLayout);
    auto* receiverForm = createFormLayout();
    m_receiverListenHostEdit = new QLineEdit(m_receiverCard);
    m_receiverListenHostEdit->setPlaceholderText(QStringLiteral("0.0.0.0"));
    m_receiverListenPortSpin = new QSpinBox(m_receiverCard);
    m_receiverListenPortSpin->setRange(1, 65535);
    m_receiverListenPortSpin->setValue(9000);
    m_receiverSaveToDiskCheck = new QCheckBox(QStringLiteral("接收数据后保存到本地"), m_receiverCard);
    m_receiverSaveToDiskCheck->setChecked(true);
    configureFieldWidget(m_receiverListenHostEdit);
    configureFieldWidget(m_receiverListenPortSpin);
    configureFieldWidget(m_receiverSaveToDiskCheck);
    receiverForm->addRow(QStringLiteral("监听 IP"), m_receiverListenHostEdit);
    receiverForm->addRow(QStringLiteral("监听端口"), m_receiverListenPortSpin);
    receiverForm->addRow(QStringLiteral("本地存储"), m_receiverSaveToDiskCheck);
    receiverLayout->addLayout(receiverForm);

    auto* receiverHint = new QLabel(QStringLiteral("接收端模式下，点击主界面的“开始采集”按钮将进入监听和接收流程。"),
                                    m_receiverCard);
    receiverHint->setObjectName("HintLabel");
    receiverHint->setWordWrap(true);
    receiverLayout->addWidget(receiverHint);

    auto* receiverButtonRow = new QHBoxLayout();
    auto* receiverApplyButton = new QPushButton(QStringLiteral("应用配置"), m_receiverCard);
    auto* receiverSaveButton = new QPushButton(QStringLiteral("保存到项目"), m_receiverCard);
    connect(receiverApplyButton, &QPushButton::clicked, this, &SettingsPage::onApplyButtonClicked);
    connect(receiverSaveButton, &QPushButton::clicked, this, &SettingsPage::onSaveProjectButtonClicked);
    receiverButtonRow->addWidget(receiverApplyButton);
    receiverButtonRow->addWidget(receiverSaveButton);
    receiverButtonRow->addStretch();
    receiverLayout->addLayout(receiverButtonRow);
    contentLayout->addWidget(m_receiverCard);

    QVBoxLayout* dioLayout = nullptr;
    m_dioCard = createCard(contentWidget, QStringLiteral("DIO 设置"), &dioLayout);
    auto* dioTop = new QHBoxLayout();
    m_dioGroupCombo = new QComboBox(m_dioCard);
    m_dioGroupCombo->addItem(QStringLiteral("Group 0"), 0);
    m_dioGroupCombo->addItem(QStringLiteral("Group 1"), 1);
    m_dioDirectionCombo = new QComboBox(m_dioCard);
    m_dioDirectionCombo->addItem(QStringLiteral("0 - 输入"), 0);
    m_dioDirectionCombo->addItem(QStringLiteral("1 - 输出"), 1);
    auto* setDirButton = new QPushButton(QStringLiteral("设置方向"), m_dioCard);
    auto* writeDoButton = new QPushButton(QStringLiteral("写 DO"), m_dioCard);
    auto* readDiButton = new QPushButton(QStringLiteral("读 DI"), m_dioCard);
    connect(setDirButton, &QPushButton::clicked, this, &SettingsPage::onSetDioDirectionClicked);
    connect(writeDoButton, &QPushButton::clicked, this, &SettingsPage::onWriteDoClicked);
    connect(readDiButton, &QPushButton::clicked, this, &SettingsPage::onReadDiClicked);
    dioTop->addWidget(new QLabel(QStringLiteral("组号"), m_dioCard));
    dioTop->addWidget(m_dioGroupCombo);
    dioTop->addWidget(new QLabel(QStringLiteral("方向"), m_dioCard));
    dioTop->addWidget(m_dioDirectionCombo);
    dioTop->addWidget(setDirButton);
    dioTop->addWidget(writeDoButton);
    dioTop->addWidget(readDiButton);
    dioTop->addStretch();
    dioLayout->addLayout(dioTop);

    auto* bitGrid = new QGridLayout();
    for (int i = 0; i < 8; ++i) {
        m_dioBits[i] = new QCheckBox(QStringLiteral("D%1").arg(i), m_dioCard);
        bitGrid->addWidget(m_dioBits[i], 0, i);
    }
    dioLayout->addLayout(bitGrid);
    contentLayout->addWidget(m_dioCard);

    m_statusLabel = new QLabel(contentWidget);
    m_statusLabel->setObjectName("HintLabel");
    contentLayout->addWidget(m_statusLabel);
    contentLayout->addStretch();

    setSettings(dsa::DsaAcquisitionSettings{});
    setNetworkSettings(dsa::DsaNetworkSettings{});
    setReceiverSettings(dsa::DsaReceiverSettings{});
    setStatusMessage(QStringLiteral("参数页已就绪。"));
    updateModeUi();
}

unsigned int SettingsPage::appMode() const {
    return static_cast<unsigned int>(m_appModeCombo->currentData().toUInt());
}

bool SettingsPage::collectSettings(dsa::DsaAcquisitionSettings& settings, QString* error) const {
    settings.appMode = static_cast<unsigned int>(m_appModeCombo->currentData().toUInt());
    settings.saveToDisk = m_saveToDiskCheck->isChecked();
    settings.sampleRateSel = static_cast<unsigned int>(m_sampleRateCombo->currentData().toUInt());
    settings.sampleMode = static_cast<unsigned int>(m_sampleModeCombo->currentData().toUInt());
    settings.triggerSource = static_cast<unsigned int>(m_triggerSourceCombo->currentData().toUInt());
    settings.externalTriggerEdge = static_cast<unsigned int>(m_externalTriggerEdgeCombo->currentData().toUInt());
    settings.clockBase = static_cast<unsigned int>(m_clockBaseCombo->currentData().toUInt());
    settings.fixedPointCountPerCh = static_cast<unsigned int>(m_fixedPointSpin->value());
    return settings.validate(error);
}

bool SettingsPage::collectNetworkSettings(dsa::DsaNetworkSettings& settings, QString* error) const {
    settings.enabled = m_enableNetworkCheck->isChecked();
    settings.remoteHost = m_remoteHostEdit->text().trimmed();
    settings.remotePort = static_cast<unsigned int>(m_remotePortSpin->value());
    return settings.validate(error);
}

bool SettingsPage::collectReceiverSettings(dsa::DsaReceiverSettings& settings, QString* error) const {
    settings.listenHost = m_receiverListenHostEdit->text().trimmed();
    settings.listenPort = static_cast<unsigned int>(m_receiverListenPortSpin->value());
    settings.saveToDisk = m_receiverSaveToDiskCheck->isChecked();
    return settings.validate(error);
}

dsa::DsaDioSettings SettingsPage::collectDioSettings() const {
    dsa::DsaDioSettings settings;
    settings.groupIndex = static_cast<unsigned int>(m_dioGroupCombo->currentData().toUInt());
    settings.direction = static_cast<unsigned int>(m_dioDirectionCombo->currentData().toUInt());

    unsigned char bits = 0;
    for (int i = 0; i < 8; ++i) {
        if (m_dioBits[i] != nullptr && m_dioBits[i]->isChecked()) {
            bits = static_cast<unsigned char>(bits | (1 << i));
        }
    }
    settings.doData = bits;
    return settings;
}

void SettingsPage::setSettings(const dsa::DsaAcquisitionSettings& settings) {
    auto selectByData = [](QComboBox* combo, unsigned int value) {
        const int index = combo->findData(value);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
    };
    selectByData(m_appModeCombo, settings.appMode);
    selectByData(m_sampleRateCombo, settings.sampleRateSel);
    selectByData(m_sampleModeCombo, settings.sampleMode);
    selectByData(m_triggerSourceCombo, settings.triggerSource);
    selectByData(m_externalTriggerEdgeCombo, settings.externalTriggerEdge);
    selectByData(m_clockBaseCombo, settings.clockBase);
    m_fixedPointSpin->setValue(static_cast<int>(settings.fixedPointCountPerCh));
    m_saveToDiskCheck->setChecked(settings.saveToDisk);
    updateModeUi();
}

void SettingsPage::setNetworkSettings(const dsa::DsaNetworkSettings& settings) {
    m_enableNetworkCheck->setChecked(settings.enabled);
    m_remoteHostEdit->setText(settings.remoteHost);
    m_remotePortSpin->setValue(static_cast<int>(settings.remotePort));
}

void SettingsPage::setReceiverSettings(const dsa::DsaReceiverSettings& settings) {
    m_receiverListenHostEdit->setText(settings.listenHost);
    m_receiverListenPortSpin->setValue(static_cast<int>(settings.listenPort));
    m_receiverSaveToDiskCheck->setChecked(settings.saveToDisk);
}

void SettingsPage::setDioInputValue(unsigned char diData) {
    for (int i = 0; i < 8; ++i) {
        const bool bit = (diData & (1 << i)) != 0;
        if (m_dioBits[i] != nullptr) {
            m_dioBits[i]->setChecked(bit);
        }
    }
}

void SettingsPage::setStatusMessage(const QString& message, bool error) {
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet(error ? "color:#AF2222;" : "color:#2D3A52;");
}

void SettingsPage::onReadButtonClicked() {
    emit readSettingsRequested();
}

void SettingsPage::onApplyButtonClicked() {
    dsa::DsaAcquisitionSettings settings;
    dsa::DsaNetworkSettings networkSettings;
    dsa::DsaReceiverSettings receiverSettings;
    QString error;
    if (!collectSettings(settings, &error)) {
        setStatusMessage(QStringLiteral("参数不合法：%1").arg(error), true);
        return;
    }
    if (!collectNetworkSettings(networkSettings, &error)) {
        setStatusMessage(QStringLiteral("网络参数不合法：%1").arg(error), true);
        return;
    }
    if (!collectReceiverSettings(receiverSettings, &error)) {
        setStatusMessage(QStringLiteral("接收端参数不合法：%1").arg(error), true);
        return;
    }
    emit applySettingsRequested(settings, networkSettings, receiverSettings);
}

void SettingsPage::onConnectNetworkButtonClicked() {
    dsa::DsaNetworkSettings networkSettings;
    QString error;
    if (!collectNetworkSettings(networkSettings, &error)) {
        setStatusMessage(QStringLiteral("网络参数不合法：%1").arg(error), true);
        return;
    }
    emit connectNetworkRequested(networkSettings);
}

void SettingsPage::onSaveProjectButtonClicked() {
    dsa::DsaAcquisitionSettings settings;
    dsa::DsaNetworkSettings networkSettings;
    dsa::DsaReceiverSettings receiverSettings;
    QString error;
    if (!collectSettings(settings, &error)) {
        setStatusMessage(QStringLiteral("参数不合法：%1").arg(error), true);
        return;
    }
    if (!collectNetworkSettings(networkSettings, &error)) {
        setStatusMessage(QStringLiteral("网络参数不合法：%1").arg(error), true);
        return;
    }
    if (!collectReceiverSettings(receiverSettings, &error)) {
        setStatusMessage(QStringLiteral("接收端参数不合法：%1").arg(error), true);
        return;
    }
    emit saveToProjectRequested(settings, collectDioSettings(), networkSettings, receiverSettings);
}

void SettingsPage::onSetDioDirectionClicked() {
    emit dioDirectionSetRequested(static_cast<unsigned int>(m_dioGroupCombo->currentData().toUInt()),
                                  static_cast<unsigned int>(m_dioDirectionCombo->currentData().toUInt()));
}

void SettingsPage::onWriteDoClicked() {
    const dsa::DsaDioSettings dioSettings = collectDioSettings();
    emit dioWriteRequested(dioSettings.groupIndex, dioSettings.doData);
}

void SettingsPage::onReadDiClicked() {
    emit dioReadRequested(static_cast<unsigned int>(m_dioGroupCombo->currentData().toUInt()));
}

void SettingsPage::onAppModeChanged() {
    updateModeUi();
    emit appModeChanged(appMode());
}

void SettingsPage::updateModeUi() {
    const bool isSenderMode =
        static_cast<unsigned int>(m_appModeCombo->currentData().toUInt()) == static_cast<unsigned int>(dsa::AppMode::Sender);
    m_senderSettingsCard->setVisible(isSenderMode);
    m_senderNetworkCard->setVisible(isSenderMode);
    m_dioCard->setVisible(isSenderMode);
    m_receiverCard->setVisible(!isSenderMode);
}
