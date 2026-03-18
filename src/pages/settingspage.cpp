#include "pages/settingspage.h"

#include "theme/themehelper.h"

#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(14);

    QFrame* settingsCard = new QFrame(this);
    settingsCard->setObjectName("CardWidget");
    ThemeHelper::applyCardShadow(settingsCard);
    QVBoxLayout* cardLayout = new QVBoxLayout(settingsCard);
    cardLayout->setContentsMargins(18, 16, 18, 16);
    cardLayout->setSpacing(12);

    QLabel* title = new QLabel("采集参数设置", settingsCard);
    title->setObjectName("CardTitle");
    cardLayout->addWidget(title);

    QFormLayout* form = new QFormLayout();
    form->setHorizontalSpacing(20);
    form->setVerticalSpacing(10);

    m_sampleRateCombo = new QComboBox(settingsCard);
    m_sampleRateCombo->addItem(dsa::sampleRateToText(0), 0);
    m_sampleRateCombo->addItem(dsa::sampleRateToText(1), 1);
    m_sampleRateCombo->addItem(dsa::sampleRateToText(2), 2);
    m_sampleRateCombo->addItem(dsa::sampleRateToText(3), 3);
    m_sampleRateCombo->addItem(dsa::sampleRateToText(4), 4);
    m_sampleRateCombo->addItem(dsa::sampleRateToText(5), 5);

    m_sampleModeCombo = new QComboBox(settingsCard);
    m_sampleModeCombo->addItem(dsa::sampleModeToText(0), 0);
    m_sampleModeCombo->addItem(dsa::sampleModeToText(1), 1);
    m_sampleModeCombo->addItem(dsa::sampleModeToText(2), 2);

    m_triggerSourceCombo = new QComboBox(settingsCard);
    m_triggerSourceCombo->addItem(dsa::triggerSourceToText(0), 0);
    m_triggerSourceCombo->addItem(dsa::triggerSourceToText(1), 1);

    m_externalTriggerEdgeCombo = new QComboBox(settingsCard);
    m_externalTriggerEdgeCombo->addItem(dsa::externalTriggerEdgeToText(0), 0);
    m_externalTriggerEdgeCombo->addItem(dsa::externalTriggerEdgeToText(1), 1);

    m_clockBaseCombo = new QComboBox(settingsCard);
    m_clockBaseCombo->addItem(dsa::clockBaseToText(0), 0);
    m_clockBaseCombo->addItem(dsa::clockBaseToText(1), 1);

    m_fixedPointSpin = new QSpinBox(settingsCard);
    m_fixedPointSpin->setRange(16, static_cast<int>(dsa::kMaxPointPerChannel));
    m_fixedPointSpin->setSingleStep(16);
    m_fixedPointSpin->setValue(4096);

    form->addRow("采样率", m_sampleRateCombo);
    form->addRow("采集模式", m_sampleModeCombo);
    form->addRow("触发源", m_triggerSourceCombo);
    form->addRow("外部触发沿", m_externalTriggerEdgeCombo);
    form->addRow("时钟基准", m_clockBaseCombo);
    form->addRow("定点每通道点数", m_fixedPointSpin);

    cardLayout->addLayout(form);

    QHBoxLayout* buttonRow = new QHBoxLayout();
    QPushButton* readButton = new QPushButton("读取当前配置", settingsCard);
    QPushButton* applyButton = new QPushButton("应用配置", settingsCard);
    QPushButton* saveButton = new QPushButton("保存到项目", settingsCard);
    connect(readButton, &QPushButton::clicked, this, &SettingsPage::onReadButtonClicked);
    connect(applyButton, &QPushButton::clicked, this, &SettingsPage::onApplyButtonClicked);
    connect(saveButton, &QPushButton::clicked, this, &SettingsPage::onSaveProjectButtonClicked);
    buttonRow->addWidget(readButton);
    buttonRow->addWidget(applyButton);
    buttonRow->addWidget(saveButton);
    buttonRow->addStretch();
    cardLayout->addLayout(buttonRow);

    rootLayout->addWidget(settingsCard);

    QFrame* dioCard = new QFrame(this);
    dioCard->setObjectName("CardWidget");
    ThemeHelper::applyCardShadow(dioCard);
    QVBoxLayout* dioLayout = new QVBoxLayout(dioCard);
    dioLayout->setContentsMargins(18, 16, 18, 16);
    dioLayout->setSpacing(10);

    QLabel* dioTitle = new QLabel("DIO 设置（两组，每组 8 位，3.3V）", dioCard);
    dioTitle->setObjectName("CardTitle");
    dioLayout->addWidget(dioTitle);

    QHBoxLayout* dioTop = new QHBoxLayout();
    m_dioGroupCombo = new QComboBox(dioCard);
    m_dioGroupCombo->addItem("Group 0", 0);
    m_dioGroupCombo->addItem("Group 1", 1);
    m_dioDirectionCombo = new QComboBox(dioCard);
    m_dioDirectionCombo->addItem("0 - 输入", 0);
    m_dioDirectionCombo->addItem("1 - 输出", 1);
    QPushButton* setDirButton = new QPushButton("设置方向", dioCard);
    QPushButton* writeDoButton = new QPushButton("写 DO", dioCard);
    QPushButton* readDiButton = new QPushButton("读 DI", dioCard);
    connect(setDirButton, &QPushButton::clicked, this, &SettingsPage::onSetDioDirectionClicked);
    connect(writeDoButton, &QPushButton::clicked, this, &SettingsPage::onWriteDoClicked);
    connect(readDiButton, &QPushButton::clicked, this, &SettingsPage::onReadDiClicked);

    dioTop->addWidget(new QLabel("组号", dioCard));
    dioTop->addWidget(m_dioGroupCombo);
    dioTop->addWidget(new QLabel("方向", dioCard));
    dioTop->addWidget(m_dioDirectionCombo);
    dioTop->addWidget(setDirButton);
    dioTop->addWidget(writeDoButton);
    dioTop->addWidget(readDiButton);
    dioTop->addStretch();
    dioLayout->addLayout(dioTop);

    QGridLayout* bitGrid = new QGridLayout();
    for (int i = 0; i < 8; ++i) {
        QCheckBox* bitCheck = new QCheckBox(QString("D%1").arg(i), dioCard);
        m_dioBits[i] = bitCheck;
        bitGrid->addWidget(bitCheck, 0, i);
    }
    dioLayout->addLayout(bitGrid);
    rootLayout->addWidget(dioCard);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("HintLabel");
    m_statusLabel->setText("参数页就绪。");
    rootLayout->addWidget(m_statusLabel);
    rootLayout->addStretch();

    setSettings(dsa::DsaAcquisitionSettings{});
}

bool SettingsPage::collectSettings(dsa::DsaAcquisitionSettings& settings, QString* error) const {
    settings.sampleRateSel = static_cast<unsigned int>(m_sampleRateCombo->currentData().toUInt());
    settings.sampleMode = static_cast<unsigned int>(m_sampleModeCombo->currentData().toUInt());
    settings.triggerSource = static_cast<unsigned int>(m_triggerSourceCombo->currentData().toUInt());
    settings.externalTriggerEdge = static_cast<unsigned int>(m_externalTriggerEdgeCombo->currentData().toUInt());
    settings.clockBase = static_cast<unsigned int>(m_clockBaseCombo->currentData().toUInt());
    settings.fixedPointCountPerCh = static_cast<unsigned int>(m_fixedPointSpin->value());
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
    selectByData(m_sampleRateCombo, settings.sampleRateSel);
    selectByData(m_sampleModeCombo, settings.sampleMode);
    selectByData(m_triggerSourceCombo, settings.triggerSource);
    selectByData(m_externalTriggerEdgeCombo, settings.externalTriggerEdge);
    selectByData(m_clockBaseCombo, settings.clockBase);
    m_fixedPointSpin->setValue(static_cast<int>(settings.fixedPointCountPerCh));
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
    QString error;
    if (!collectSettings(settings, &error)) {
        setStatusMessage(QString("参数不合法：%1").arg(error), true);
        return;
    }
    emit applySettingsRequested(settings);
}

void SettingsPage::onSaveProjectButtonClicked() {
    dsa::DsaAcquisitionSettings settings;
    QString error;
    if (!collectSettings(settings, &error)) {
        setStatusMessage(QString("参数不合法：%1").arg(error), true);
        return;
    }
    emit saveToProjectRequested(settings, collectDioSettings());
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
