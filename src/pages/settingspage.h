#pragma once

#include "core/dsa16chtypes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>
#include <array>

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

    bool collectSettings(dsa::DsaAcquisitionSettings& settings, QString* error) const;
    dsa::DsaDioSettings collectDioSettings() const;

    void setSettings(const dsa::DsaAcquisitionSettings& settings);
    void setDioInputValue(unsigned char diData);
    void setStatusMessage(const QString& message, bool error = false);

signals:
    void readSettingsRequested();
    void applySettingsRequested(const dsa::DsaAcquisitionSettings& settings);
    void saveToProjectRequested(const dsa::DsaAcquisitionSettings& settings, const dsa::DsaDioSettings& dioSettings);
    void dioDirectionSetRequested(unsigned int groupIndex, unsigned int direction);
    void dioWriteRequested(unsigned int groupIndex, unsigned char doData);
    void dioReadRequested(unsigned int groupIndex);

private slots:
    void onReadButtonClicked();
    void onApplyButtonClicked();
    void onSaveProjectButtonClicked();
    void onSetDioDirectionClicked();
    void onWriteDoClicked();
    void onReadDiClicked();

private:
    QComboBox* m_sampleRateCombo = nullptr;
    QComboBox* m_sampleModeCombo = nullptr;
    QComboBox* m_triggerSourceCombo = nullptr;
    QComboBox* m_externalTriggerEdgeCombo = nullptr;
    QComboBox* m_clockBaseCombo = nullptr;
    QSpinBox* m_fixedPointSpin = nullptr;

    QComboBox* m_dioGroupCombo = nullptr;
    QComboBox* m_dioDirectionCombo = nullptr;
    std::array<QCheckBox*, 8> m_dioBits{};

    QLabel* m_statusLabel = nullptr;
};
