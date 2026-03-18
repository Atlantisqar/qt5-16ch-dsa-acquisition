#pragma once

#include "models/projectfilemodel.h"

#include <QLabel>
#include <QListWidget>
#include <QWidget>

class HomePage : public QWidget {
    Q_OBJECT

public:
    explicit HomePage(QWidget* parent = nullptr);

    void setProjectInfo(const ProjectFileModel* project);
    void setSdkStatus(bool ready, const QString& message);
    void setDeviceStatus(bool opened);
    void setRecentProjects(const QStringList& recentProjects);

private:
    QLabel* m_projectNameLabel = nullptr;
    QLabel* m_projectPathLabel = nullptr;
    QLabel* m_sdkStatusLabel = nullptr;
    QLabel* m_deviceStatusLabel = nullptr;
    QListWidget* m_recentList = nullptr;
};
