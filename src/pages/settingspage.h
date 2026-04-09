#pragma once

#include "core/dsa16chtypes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>
#include <array>

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);
    unsigned int appMode() const;

    bool collectSettings(dsa::DsaAcquisitionSettings& settings, QString* error) const;
    bool collectNetworkSettings(dsa::DsaNetworkSettings& settings, QString* error) const;
    bool collectReceiverSettings(dsa::DsaReceiverSettings& settings, QString* error) const;
    dsa::DsaDioSettings collectDioSettings() const;

    void setSettings(const dsa::DsaAcquisitionSettings& settings);
    void setNetworkSettings(const dsa::DsaNetworkSettings& settings);
    void setReceiverSettings(const dsa::DsaReceiverSettings& settings);
    void setDioInputValue(unsigned char diData);
    void setStatusMessage(const QString& message, bool error = false);

signals:
    void appModeChanged(unsigned int appMode);
    void readSettingsRequested();
    void applySettingsRequested(const dsa::DsaAcquisitionSettings& settings,
                                const dsa::DsaNetworkSettings& networkSettings,
                                const dsa::DsaReceiverSettings& receiverSettings);
    void connectNetworkRequested(const dsa::DsaNetworkSettings& networkSettings);
    void saveToProjectRequested(const dsa::DsaAcquisitionSettings& settings,
                                const dsa::DsaDioSettings& dioSettings,
                                const dsa::DsaNetworkSettings& networkSettings,
                                const dsa::DsaReceiverSettings& receiverSettings);
    void dioDirectionSetRequested(unsigned int groupIndex, unsigned int direction);
    void dioWriteRequested(unsigned int groupIndex, unsigned char doData);
    void dioReadRequested(unsigned int groupIndex);

private slots:
    void onReadButtonClicked();
    void onApplyButtonClicked();
    void onConnectNetworkButtonClicked();
    void onSaveProjectButtonClicked();
    void onSetDioDirectionClicked();
    void onWriteDoClicked();
    void onReadDiClicked();
    void onAppModeChanged();

private:
    void updateModeUi();

private:
    QComboBox* m_appModeCombo = nullptr;

    QFrame* m_senderSettingsCard = nullptr;
    QFrame* m_senderNetworkCard = nullptr;
    QFrame* m_receiverCard = nullptr;
    QFrame* m_dioCard = nullptr;

    QComboBox* m_sampleRateCombo = nullptr;
    QComboBox* m_sampleModeCombo = nullptr;
    QComboBox* m_triggerSourceCombo = nullptr;
    QComboBox* m_externalTriggerEdgeCombo = nullptr;
    QComboBox* m_clockBaseCombo = nullptr;
    QSpinBox* m_fixedPointSpin = nullptr;
    QCheckBox* m_saveToDiskCheck = nullptr;

    QCheckBox* m_enableNetworkCheck = nullptr;
    QLineEdit* m_remoteHostEdit = nullptr;
    QSpinBox* m_remotePortSpin = nullptr;

    QLineEdit* m_receiverListenHostEdit = nullptr;
    QSpinBox* m_receiverListenPortSpin = nullptr;
    QCheckBox* m_receiverSaveToDiskCheck = nullptr;

    QComboBox* m_dioGroupCombo = nullptr;
    QComboBox* m_dioDirectionCombo = nullptr;
    std::array<QCheckBox*, 8> m_dioBits{};

    QLabel* m_statusLabel = nullptr;
};
