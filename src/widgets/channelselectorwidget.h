#pragma once

#include <QCheckBox>
#include <QPushButton>
#include <QVector>
#include <QWidget>
#include <array>

class ChannelSelectorWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChannelSelectorWidget(QWidget* parent = nullptr);

    QVector<int> selectedChannels() const;
    void setSelectedChannels(const QVector<int>& channels);

signals:
    void selectedChannelsChanged(const QVector<int>& channels);

private slots:
    void onAnyCheckboxToggled(bool checked);
    void onSelectAllClicked();
    void onClearAllClicked();

private:
    std::array<QCheckBox*, 16> m_channelChecks{};
    QPushButton* m_selectAllButton = nullptr;
    QPushButton* m_clearAllButton = nullptr;
};
